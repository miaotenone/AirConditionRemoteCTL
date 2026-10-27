#include "dht_sensor.h"

#include <Arduino.h>
#include <math.h>
#include <DHTesp.h>

#include "config.h"
#include "pins.h"

static DHTesp s_dht;
static bool s_ready = false;

void dht_begin() {
    s_dht.setup(PIN_DHT, DHTesp::DHT11);
    s_ready = true;
    Serial.printf("[dht] DHT11 on GPIO%d\n", PIN_DHT);
}

bool dht_read(int& temp_c, int& humidity) {
    if (!s_ready) return false;

    TempAndHumidity r = s_dht.getTempAndHumidity();
    if (isnan(r.temperature) || isnan(r.humidity)) return false;

    // Plausibility gate. The DHT11 itself only reports 0..50 degC / 20..90% RH,
    // so anything outside a slightly wider band is a bus glitch, not weather.
    // The floor is the one that bit us: a module with nothing to say answers an
    // all-zero frame, and that frame passes the checksum -- so 0% RH is the
    // absence of a reading, not a dry room. One of these reached the history
    // looking like a real 0 degC morning and dragged the chart down with it.
    if (r.temperature < -20.0f || r.temperature > 60.0f) return false;
    if (r.humidity < 5.0f || r.humidity > 100.0f) return false;

    temp_c = (int)lroundf(r.temperature);
    humidity = (int)lroundf(r.humidity);
    return true;
}
