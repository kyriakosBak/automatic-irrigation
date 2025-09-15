#include "scheduler.h"
#include "logger.h"
#include <Arduino.h>
#include "config/config.h"
#include <time.h>

extern float dosing_ml[NUM_FERTILIZERS];
extern int schedule_hour;
extern int schedule_minute;

unsigned long last_run = 0;
bool has_run_today = false;

void trigger_dosing(); // Implemented in main.cpp

void scheduler_init() {
    last_run = 0;
    has_run_today = false;
    configTime(0, 0, "pool.ntp.org");
    logger_log("Scheduler initialized - waiting for NTP sync");
    time_t now = 0;
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 20) {
        delay(500);
        now = time(nullptr);
        retries++;
    }
    if (now < 8 * 3600 * 2) {
        logger_log("WARNING: NTP sync failed - scheduling may be inaccurate");
    } else {
        logger_log("NTP time synchronized successfully");
    }
}

void scheduler_run() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (!localtime_r(&now, &timeinfo)) {
        logger_log("ERROR: Failed to get current time for scheduling");
        return;
    }
    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;
    if (hour == schedule_hour && min == schedule_minute && !has_run_today) {
        String log_msg = "Scheduled watering triggered at " + String(hour) + ":" + String(min);
        logger_log(log_msg.c_str());
        trigger_dosing();
        has_run_today = true;
        last_run = millis();
    }
    if (hour != schedule_hour || min != schedule_minute) {
        has_run_today = false;
    }
} 