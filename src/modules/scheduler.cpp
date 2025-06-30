#include "scheduler.h"
#include <Arduino.h>
#include "config/config.h"
#include <time.h>

extern float dosing_ml[NUM_FERTILIZERS];
extern int schedule_hour;
extern int schedule_minute;

unsigned long last_run = 0;
bool has_run_today = false;

void trigger_dosing(); // To be implemented in pump_control or main

void scheduler_init() {
    last_run = 0;
    has_run_today = false;
    configTime(0, 0, "pool.ntp.org");
    Serial.println("Waiting for NTP time...");
    time_t now = 0;
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 20) {
        delay(500);
        now = time(nullptr);
        retries++;
    }
    if (now < 8 * 3600 * 2) {
        Serial.println("NTP time not set, scheduling may be inaccurate");
    } else {
        Serial.println("NTP time set");
    }
}

void scheduler_run() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (!localtime_r(&now, &timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;
    if (hour == schedule_hour && min == schedule_minute && !has_run_today) {
        trigger_dosing();
        has_run_today = true;
        last_run = millis();
    }
    if (hour != schedule_hour || min != schedule_minute) {
        has_run_today = false;
    }
} 