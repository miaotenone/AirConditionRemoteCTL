#pragma once
#include <stdint.h>

// DHT11 room sensor on PIN_DHT (see pins.h / docs/wiring.md).
//
// The DHT11 is a budget part: 1 degC resolution, +/-2 degC accuracy, +/-5% RH,
// and it needs >= 1s between reads. That is why the firmware only samples it
// every DHT_READ_MS and why the service's closed loop uses a +/-1 degC deadband
// (design section 7.2) -- anything tighter would chase the sensor's own noise.
//
// Kept behind two calls so the self-test firmware (which must not pull the DHT
// library in) can simply leave the .cpp out of its build.

// Configure the pin. Safe to call before the sensor is even powered.
void dht_begin();

// Read once. Returns true only when the sensor answered *and* the values are
// in a physically plausible range; on false, temp_c/humidity are left alone.
bool dht_read(int& temp_c, int& humidity);
