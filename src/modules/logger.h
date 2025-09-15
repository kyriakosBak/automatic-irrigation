#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

// Configuration
#define LOG_FILE_PATH "/logs.txt"
#define LOG_FILE_BACKUP_PATH "/logs_old.txt"
#define MAX_LOG_FILE_SIZE 50000  // 50KB

// Initialize logger
void logger_init();

// Main logging function
void logger_log(const char* message);

// Log management
String logger_get_logs(int max_lines = 100);
void logger_clear();
size_t logger_get_file_size();

// Internal functions
void rotate_log_if_needed();
String get_timestamp();

#endif
