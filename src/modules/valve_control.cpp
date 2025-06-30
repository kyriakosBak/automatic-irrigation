#include "valve_control.h"
#include <Arduino.h>
#include "config/config.h"

#define VALVE_OPEN HIGH
#define VALVE_CLOSED LOW

static bool valve_open = false;

void valve_control_init() {
    pinMode(VALVE_PIN, OUTPUT);
    digitalWrite(VALVE_PIN, VALVE_CLOSED);
    valve_open = false;
}

void valve_control_run() {
    // No periodic logic; control via fill/stop functions
}

void valve_control_fill_main_tank() {
    Serial.println("[DEBUG] valve_control_fill_main_tank: Opening valve");
    digitalWrite(VALVE_PIN, VALVE_OPEN);
    valve_open = true;
}

void valve_control_stop_main_tank() {
    Serial.println("[DEBUG] valve_control_stop_main_tank: Closing valve");
    digitalWrite(VALVE_PIN, VALVE_CLOSED);
    valve_open = false;
}