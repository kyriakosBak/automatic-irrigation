#pragma once
// Configuration constants
#define NUM_PUMPS 7
#define NUM_FERTILIZERS 5
#define WATERING_PUMP_CHANNEL 6
#define VALVE_PIN 13 // GPIO pin for solenoid valve
#define HUMIDIFIER_PUMP_CHANNEL 7 // Peristaltic pump for humidifier tank
#define LIQUID_SENSOR_PIN 32 // Capacitive liquid sensor pin
#define MAIN_TANK_FILL_TIMEOUT_MS 6000 // Default: 1 minute, can be changed