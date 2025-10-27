#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

// Configuration
#define LOG_FILE_PATH "/logs.txt"
#define LOG_FILE_BACKUP_PATH "/logs_old.txt"
#define MAX_LOG_FILE_SIZE 50000  // 50KB
#define LOG_QUEUE_SIZE 100  // Maximum number of queued log entries
#define MAX_LOG_ENTRY_SIZE 256  // Maximum size of a single log entry

// Initialize logger
void logger_init();

// Main logging function (now queues the log)
void logger_log(const char* message);

// Process queued logs - call this regularly from main loop
void logger_process_queue();

// Force flush all queued logs immediately - call before critical operations
void logger_flush();

// Log management
String logger_get_logs(int max_lines = 100);
void logger_clear();
size_t logger_get_file_size();

// Queue statistics
int logger_get_queue_count();
unsigned long logger_get_dropped_count();
unsigned long logger_get_written_count();

// Internal functions
void rotate_log_if_needed();
String get_timestamp();

#endif
