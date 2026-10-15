// Hardware bring-up self-test firmware.
//
// Compiled only when the `selftest` PlatformIO env is used (-DAC_SELFTEST=1).
// It does NOT use WiFi/MQTT at all: it opens a serial menu so you can prove,
// one piece at a time, that the OLED and the IR module work.
//
//   pio run -e selftest -t upload
//   pio device monitor
//
// (Ports auto-detect; pass --upload-port / -p if several USB serial devices
//  are attached. The COM number changes when the board is replugged.)
//
#ifdef AC_SELFTEST

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "pins.h"
#include "ac_encoder.h"
#include "ac_state.h"
#include "ir_module.h"
#include "display.h"

static AcState g_state;
static uint8_t g_brand = BRAND_MIDEA1;
static char    g_line[80];
static size_t  g_len = 0;

static const char* mode_str(uint8_t m) {
    static const char* t[] = {"AUTO", "COOL", "DRY", "FAN", "HEAT"};
    return (m <= AC_MODE_HEAT) ? t[m] : "?";
}

static const char* fan_str(uint8_t f) {
    static const char* t[] = {"AUTO", "LOW", "MED", "HIGH"};
    return (f <= AC_FAN_HIGH) ? t[f] : "?";
}

static void print_state() {
    Serial.printf("[state] brand=0x%02X power=%d mode=%d(%s) temp=%d fan=%d(%s)\n",
                  g_brand, g_state.power, g_state.mode, mode_str(g_state.mode),
                  g_state.temp, g_state.fan, fan_str(g_state.fan));
}

static void i2c_scan_pins(uint8_t sda, uint8_t scl, uint32_t freq, const char* label) {
    Wire.end();
    Wire.begin(sda, scl, freq);
    Serial.printf("[i2c] scanning 0x08..0x77 (%s) ...\n", label);
    int found = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[i2c]   found 0x%02X%s\n", a,
                          (a == 0x3C || a == 0x3D) ? "   <- likely OLED" : "");
            ++found;
        }
    }
    if (!found) {
        Serial.println("[i2c]   nothing found -> check OLED VCC/GND/SDA/SCL");
    }
    Wire.end();
}

static void i2c_scan() {
    i2c_scan_pins(PIN_OLED_SDA, PIN_OLED_SCL, 100000, "SDA=GPIO21 SCL=GPIO22");
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
}

//: The same scan with the two signals the other way round. A module wired
//: backwards is invisible from the outside -- its pull-ups still hold both
//: lines high, so the cable "looks" fine, but its controller is watching what
//: it thinks is its data pin for a clock it will never get, so it never ACKs.
//: Finding the screen here means the answer is to swap two wires.
static void i2c_scan_swapped() {
    i2c_scan_pins(PIN_OLED_SCL, PIN_OLED_SDA, 100000, "SWAPPED: SDA=GPIO22 SCL=GPIO21");
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Serial.println("[i2c] bus back to SDA=GPIO21 SCL=GPIO22");
}

//: The same scan at progressively slower clocks. Weak pull-ups, long jumper
//: wires or a breadboard all round off the edges; if they get slow enough, a
//: module misses bits at 100 kHz and answers nothing -- which looks exactly
//: like a dead one. If the screen turns up here, the bus is simply too slow-
//: edged to run at full speed, and the fix is a slower clock or a stronger
//: pull-up, not a new screen.
static void i2c_scan_clocks() {
    const uint32_t freqs[] = {100000, 50000, 25000, 10000, 5000};
    char label[64];
    for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); ++i) {
        snprintf(label, sizeof(label), "SDA=GPIO21 SCL=GPIO22 @ %lu Hz",
                 (unsigned long)freqs[i]);
        i2c_scan_pins(PIN_OLED_SDA, PIN_OLED_SCL, freqs[i], label);
    }
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Serial.println("[i2c] bus back to 100000 Hz");
}

//: Is the module really on the bus? An I2C scan only ever says "nothing
//: answered", which is the same answer for a module that is missing, one that
//: is unpowered, and one plugged into the wrong header. With the ESP32's own
//: pull-ups switched off, the only thing left that can hold a line high is the
//: module's own pull-up resistor -- so a line that reads low or flaps has
//: nothing on the far end of it.
//: Drive a line low, let go, and time how long it takes to climb back up.
//: 'internal_pull' picks whether the ESP32's own ~45k pull-up helps: false
//: measures the module's resistor on its own, true measures the bus the way
//: I2C will actually see it.
static int rise_us(uint8_t pin, bool internal_pull) {
    pinMode(pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(pin, LOW);
    delayMicroseconds(50);
    pinMode(pin, internal_pull ? INPUT_PULLUP : INPUT);
    uint32_t t0 = micros();
    while (micros() - t0 < 2000) {
        if (digitalRead(pin) == HIGH) return (int)(micros() - t0);
    }
    return -1;
}

static void wire_check() {
    Wire.end();                        // hand the pins back before poking them

    for (int i = 0; i < 2; ++i) {
        uint8_t     pin = (i == 0) ? PIN_OLED_SDA : PIN_OLED_SCL;
        const char* n   = (i == 0) ? "SDA" : "SCL";

        pinMode(pin, INPUT_PULLUP);     // expect 1: neither shorted nor held low
        delay(2);
        int pulled = digitalRead(pin);

        // Reading a pin without discharging it first proves nothing: a floating
        // pin keeps its charge for milliseconds. Time the climb back instead.
        int alone = rise_us(pin, false);   // the module's pull-up by itself
        int real  = rise_us(pin, true);    // plus ours: the edge I2C gets

        Serial.printf("[wire] %s GPIO%d: idle=%d | module's pull-up alone -> %d us | with ours too -> %d us\n",
                      n, pin, pulled, alone, real);
        pinMode(pin, INPUT_PULLUP);
    }

    Serial.println("[wire] -1 = never rises. 'alone' in a few us -> module on that wire, powered.");
    Serial.println("[wire] 'with ours too' above ~1 us -> edges too slow for 100 kHz: try q.");

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
}

static void send_current(const char* why) {
    uint8_t frame[MULTI_PARAM_FRAME_SIZE];
    size_t n = build_multi_param_frame(frame, g_brand, g_state.power,
                                       g_state.mode, g_state.temp, g_state.fan);
    Serial.printf("[ir ] sending frame (%s)\n", why);
    ir_module_send(frame, n);
    print_state();
    display_show_ac(g_state, "IR SENT", "");
}

static void print_help() {
    Serial.println("--- commands (letter, then Enter; value after a space) ---");
    Serial.println("  h / ?       this help + current state");
    Serial.println("  i           I2C scan (find the OLED)");
    Serial.println("  w           wire check (is the OLED module actually there?)");
    Serial.println("  x           I2C scan with SDA/SCL swapped (cable in backwards?)");
    Serial.println("  q           I2C scan at slower clocks (edges too slow for 100 kHz?)");
    Serial.println("  o           OLED test patterns");
    Serial.println("  d           dump current state");
    Serial.println("  r           send REFERENCE frame (midea1 on/cool/24/auto)");
    Serial.println("  a           send current state as a frame");
    Serial.println("  p           toggle power  + send");
    Serial.println("  t <16-30>   set temp      + send");
    Serial.println("  m <0-4>     set mode  0auto 1cool 2dry 3fan 4heat  + send");
    Serial.println("  f <0-3>     set fan   0auto 1low 2med 3high        + send");
    Serial.println("  b <hex>     set brand (e.g. 'b 10')                + send");
    Serial.println("  l <0-15>    send LEARN frame for slot");
    Serial.println("  s <0-15>    send REPLAY frame for slot");
    Serial.println("----------------------------------------------------------");
}

static void handle_line(char* line) {
    while (*line == ' ') ++line;
    if (*line == '\0') return;

    char cmd = (char)tolower((unsigned char)line[0]);
    char* arg = line + 1;
    while (*arg == ' ') ++arg;
    int v = atoi(arg);

    switch (cmd) {
        case 'h':
        case '?':
            print_help();
            print_state();
            break;

        case 'i':
            i2c_scan();
            break;

        case 'w':
            wire_check();
            break;

        case 'x':
            i2c_scan_swapped();
            break;

        case 'q':
            i2c_scan_clocks();
            break;

        case 'o':
            Serial.println("[oled] running test patterns (watch the screen)...");
            display_test_patterns();
            display_show_ac(g_state, "OLED OK", "");
            Serial.println("[oled] done. Garbled/blank -> try the other OLED_SH1106 value.");
            break;

        case 'd':
            print_state();
            break;

        case 'r': {
            uint8_t frame[MULTI_PARAM_FRAME_SIZE];
            build_multi_param_frame(frame, BRAND_MIDEA1, AC_POWER_ON,
                                    AC_MODE_COOL, 24, AC_FAN_AUTO);
            Serial.println("[ir ] sending REFERENCE frame");
            ir_module_send(frame, MULTI_PARAM_FRAME_SIZE);
            break;
        }

        case 'a':
            send_current("current state");
            break;

        case 'p':
            g_state.power = g_state.power ? AC_POWER_OFF : AC_POWER_ON;
            send_current("power toggle");
            break;

        case 't':
            ac_state_apply_field(g_state, "temp", v);
            send_current("set temp");
            break;

        case 'm':
            ac_state_apply_field(g_state, "mode", v);
            send_current("set mode");
            break;

        case 'f':
            ac_state_apply_field(g_state, "fan", v);
            send_current("set fan");
            break;

        case 'b':
            g_brand = (uint8_t)strtol(arg, nullptr, 16);
            send_current("set brand");
            break;

        case 'l': {
            uint8_t f[LEARN_FRAME_SIZE];
            build_learn_frame(f, (uint8_t)v);
            Serial.printf("[ir ] LEARN slot %d\n", v);
            ir_module_send(f, LEARN_FRAME_SIZE);
            break;
        }

        case 's': {
            uint8_t f[LEARN_FRAME_SIZE];
            build_send_learned_frame(f, (uint8_t)v);
            Serial.printf("[ir ] REPLAY slot %d\n", v);
            ir_module_send(f, LEARN_FRAME_SIZE);
            break;
        }

        default:
            Serial.printf("[cmd] unknown '%c' (type h for help)\n", cmd);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(400);

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("  AC GATEWAY  --  HARDWARE SELF TEST");
    Serial.printf ("  chip  : %s rev%d  %d core(s) @ %d MHz\n",
                   ESP.getChipModel(), ESP.getChipRevision(),
                   ESP.getChipCores(), getCpuFrequencyMhz());
    Serial.printf ("  flash : %u KB\n", (unsigned)(ESP.getFlashChipSize() / 1024));
    Serial.printf ("  build : %s %s\n", __DATE__, __TIME__);
#if OLED_SH1106
    Serial.println("  OLED  : driver SH1106  (config.h OLED_SH1106=1)");
#else
    Serial.println("  OLED  : driver SSD1306 (config.h OLED_SH1106=0)");
#endif
    Serial.printf ("  OLED  : I2C SDA=GPIO%d SCL=GPIO%d\n", PIN_OLED_SDA, PIN_OLED_SCL);
    Serial.printf ("  IR    : UART RX=GPIO%d TX=GPIO%d @ %d 8N1\n",
                   PIN_IR_RX, PIN_IR_TX, IR_BAUD);
    Serial.println("==================================================");

    i2c_scan();

    display_begin();
    Serial.printf("[oled] active address = 0x%02X\n", display_address());
    display_show_ac(g_state, "READY", "");
    display_test_patterns();
    display_show_ac(g_state, "READY", "");

    ir_module_begin();

    Serial.println();
    print_help();
    print_state();
    Serial.print("selftest> ");
}

void loop() {
    // Echo anything the IR module sends back (some modules ACK).
    while (Serial2.available()) {
        int b = Serial2.read();
        Serial.printf("\n[ir ] <- 0x%02X\n", b);
        Serial.print("selftest> ");
    }

    // Serial command line (with local echo, since the monitor does not echo).
    while (Serial.available()) {
        int ci = Serial.read();
        if (ci < 0) break;
        char c = (char)ci;

        if (c == '\r' || c == '\n') {
            if (g_len > 0) {
                g_line[g_len] = '\0';
                Serial.println();
                handle_line(g_line);
                g_len = 0;
            }
            Serial.print("selftest> ");
        } else if (c == 8 || c == 127) {  // backspace
            if (g_len > 0) {
                --g_len;
                Serial.print("\b \b");
            }
        } else if (c >= 0x20 && g_len < sizeof(g_line) - 1) {
            g_line[g_len++] = c;
            Serial.write(c);  // local echo
        }
    }

    delay(2);
}

#endif  // AC_SELFTEST
