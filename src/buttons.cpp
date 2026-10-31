#include "buttons.h"
#include <Arduino.h>
#include "pins.h"
#include "display.h"
#include "ui_navigation.h"

static ButtonState s_buttons[3];
static const int s_button_pins[] = {PIN_BUTTON_LEFT, PIN_BUTTON_OK, PIN_BUTTON_RIGHT};
static bool s_chord = false;
static bool s_releasing = false;
static uint32_t s_release_since = 0;

void buttons_begin() {
    const uint32_t now = millis();
    for (unsigned i = 0; i < 3; ++i) {
        pinMode(s_button_pins[i], INPUT_PULLUP);
        s_buttons[i].reset(digitalRead(s_button_pins[i]) == LOW, now);
    }
    s_chord = false;
    s_releasing = false;
}

void buttons_loop() {
    const uint32_t now = millis();
    bool pressed[3];
    unsigned count = 0;
    for (unsigned i = 0; i < 3; ++i) {
        pressed[i] = digitalRead(s_button_pins[i]) == LOW;
        if (pressed[i]) ++count;
    }
    // No combination shortcuts: drop the whole gesture, including its release.
    if (count > 1) s_chord = true;
    if (s_chord) {
        if (count != 0) {
            s_releasing = false;
        } else if (!s_releasing) {
            s_releasing = true;
            s_release_since = now;
        } else if ((uint32_t)(now - s_release_since) >= 30) {
            for (auto& button : s_buttons)
                button.reset(false, now);
            s_chord = s_releasing = false;
        }
        return;
    }
    for (unsigned i = 0; i < 3; ++i) {
        ButtonPress event = s_buttons[i].sample(pressed[i], now, i == 1);
        if (event == ButtonPress::none) continue;
        if (i == 1)
            display_navigate(event == ButtonPress::long_press ? UiEvent::ok_long
                                                              : UiEvent::ok_short);
        else
            display_navigate(i == 0 ? UiEvent::left : UiEvent::right);
    }
}
