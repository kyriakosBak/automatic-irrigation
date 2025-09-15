#include "sensors.h"
#include "logger.h"
#include <Arduino.h>
#include "config/config.h"

static bool liquid_level = false;
static bool last_liquid_level = false;

void sensors_init() {
    pinMode(LIQUID_SENSOR_PIN, INPUT);
    logger_log("Sensors initialized");
}

void sensors_read() {
    last_liquid_level = liquid_level;
    liquid_level = digitalRead(LIQUID_SENSOR_PIN);
    
    // Log only when liquid level changes
    if (liquid_level != last_liquid_level) {
        String log_msg = "Liquid level changed: " + String(liquid_level ? "PRESENT" : "NOT PRESENT");
        logger_log(log_msg.c_str());
    }
}

bool sensors_get_liquid_level() {
    return liquid_level;
} 