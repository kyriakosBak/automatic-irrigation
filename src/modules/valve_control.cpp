#include "valve_control.h"
#include "logger.h"
#include <Arduino.h>
#include "config/config.h"

#define VALVE_OPEN HIGH
#define VALVE_CLOSED LOW

static bool valve_open = false;

void valve_control_init() {
    pinMode(VALVE_PIN, OUTPUT);
    digitalWrite(VALVE_PIN, VALVE_CLOSED);
    valve_open = false;
    logger_log("Valve control initialized - valve closed");
}

void valve_control_fill_main_tank() {
    logger_log("Main tank valve opened - filling started");
    digitalWrite(VALVE_PIN, VALVE_OPEN);
    valve_open = true;
}

void valve_control_stop_main_tank() {
    logger_log("Main tank valve closed - filling stopped");
    digitalWrite(VALVE_PIN, VALVE_CLOSED);
    valve_open = false;
}