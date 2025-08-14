#pragma once
// Configuration constants
#define NUM_PUMPS 7
#define NUM_FERTILIZERS 5
#define MAIN_TANK_CHANNEL 6
#define VALVE_PIN 25 // GPIO pin for solenoid valve
#define AUX_PUMP_CHANNEL 7 // Peristaltic pump for auxiliary tank
#define LIQUID_SENSOR_PIN 32 // Capacitive liquid sensor pin
#define MAIN_TANK_FILL_TIMEOUT_MS 6000 // Default: 1 minute, can be changed