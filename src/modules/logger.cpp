#include "logger.h"
#include <LittleFS.h>

void logger_init() {
    // Logger initialization
    Serial.println("Logger initialized");
    
    // Create initial log entry if LittleFS is available
    if (LittleFS.begin()) {
        logger_log("Logger system initialized");
        logger_log("System startup");
    }
}

String get_timestamp() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo)) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buf);
    }
    return String(millis()); // Fallback to millis if NTP not synced
}

void rotate_log_if_needed() {
    File logFile = LittleFS.open(LOG_FILE_PATH, "r");
    if (!logFile) {
        return; // No current log file, nothing to rotate
    }
    
    size_t fileSize = logFile.size();
    logFile.close();
    
    if (fileSize >= MAX_LOG_FILE_SIZE) {
        // Delete old backup if it exists
        if (LittleFS.exists(LOG_FILE_BACKUP_PATH)) {
            LittleFS.remove(LOG_FILE_BACKUP_PATH);
        }
        
        // Move current log to backup
        LittleFS.rename(LOG_FILE_PATH, LOG_FILE_BACKUP_PATH);
        
        Serial.println("Log file rotated");
    }
}

void logger_log(const char* message) {
    rotate_log_if_needed();
    
    File logFile = LittleFS.open(LOG_FILE_PATH, "a");
    if (!logFile) {
        Serial.println("Failed to open log file for writing: " + String(LOG_FILE_PATH));
        return;
    }
    
    String timestamp = get_timestamp();
    String logEntry = "[" + timestamp + "] " + String(message) + "\n";
    
    size_t written = logFile.print(logEntry);
    logFile.flush(); // Force write to filesystem
    logFile.close();
    
    // Debug output
    Serial.println("LOG: " + logEntry.substring(0, logEntry.length()-1)); // Remove the \n for serial
    Serial.println("DEBUG: Wrote " + String(written) + " bytes to log file");
}

String logger_get_logs(int max_lines) {
    File logFile = LittleFS.open(LOG_FILE_PATH, "r");
    if (!logFile) {
        return "No log file found";
    }
    
    size_t fileSize = logFile.size();
    if (fileSize == 0) {
        logFile.close();
        return "Log file is empty";
    }
    
    // For large files, read only the last portion
    String result = "";
    if (max_lines > 0 && fileSize > 10000) {
        // Read last 10KB for large files
        size_t readSize = min(fileSize, (size_t)10000);
        logFile.seek(fileSize - readSize);
        
        // Skip partial first line
        while (logFile.available() && logFile.read() != '\n');
        
        // Read the rest
        while (logFile.available()) {
            char c = logFile.read();
            result += c;
        }
    } else {
        // Read entire file for small files
        while (logFile.available()) {
            char c = logFile.read();
            result += c;
        }
    }
    
    logFile.close();
    
    if (result.length() == 0) {
        return "Unable to read log contents";
    }
    
    return result;
}

void logger_clear() {
    if (LittleFS.exists(LOG_FILE_PATH)) {
        LittleFS.remove(LOG_FILE_PATH);
    }
    if (LittleFS.exists(LOG_FILE_BACKUP_PATH)) {
        LittleFS.remove(LOG_FILE_BACKUP_PATH);
    }
    Serial.println("Log files cleared");
}

size_t logger_get_file_size() {
    File logFile = LittleFS.open(LOG_FILE_PATH, "r");
    if (!logFile) {
        return 0;
    }
    size_t size = logFile.size();
    logFile.close();
    return size;
}
