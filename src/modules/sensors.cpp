#include "sensors.h"
#include <Arduino.h>
#include "config/config.h"

static bool liquid_level = false;

void sensors_init() {
    pinMode(LIQUID_SENSOR_PIN, INPUT);
}

void sensors_read() {
    liquid_level = digitalRead(LIQUID_SENSOR_PIN);
    Serial.print("Liquid sensor: ");
    Serial.println(liquid_level ? "PRESENT" : "NOT PRESENT");
}

bool sensors_get_liquid_level() {
    return liquid_level;
} 