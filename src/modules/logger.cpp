#include "logger.h"

void logger_init() {
    // Logger initialization
    Serial.println("Logger initialized");
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
        Serial.println("Failed to open log file for writing");
        return;
    }
    
    String timestamp = get_timestamp();
    String logEntry = "[" + timestamp + "] " + String(message) + "\n";
    
    logFile.print(logEntry);
    logFile.close();
    
    // Also print to Serial for debugging
    Serial.print("LOG: " + logEntry);
}

String logger_get_logs(int max_lines) {
    String result = "";
    int lineCount = 0;
    
    // Read current log file
    File logFile = LittleFS.open(LOG_FILE_PATH, "r");
    if (logFile) {
        // Count total lines first
        int totalLines = 0;
        while (logFile.available()) {
            if (logFile.read() == '\n') totalLines++;
        }
        logFile.seek(0);
        
        // Skip lines if we have more than max_lines
        int linesToSkip = (totalLines > max_lines) ? totalLines - max_lines : 0;
        int currentLine = 0;
        
        String line = "";
        while (logFile.available()) {
            char c = logFile.read();
            if (c == '\n') {
                currentLine++;
                if (currentLine > linesToSkip) {
                    result += line + "\n";
                    lineCount++;
                }
                line = "";
            } else {
                line += c;
            }
        }
        // Add last line if it doesn't end with newline
        if (line.length() > 0 && currentLine >= linesToSkip) {
            result += line + "\n";
        }
        logFile.close();
    }
    
    // If we still need more lines and have backup file, read from it
    if (lineCount < max_lines && LittleFS.exists(LOG_FILE_BACKUP_PATH)) {
        File backupFile = LittleFS.open(LOG_FILE_BACKUP_PATH, "r");
        if (backupFile) {
            String backupContent = "";
            while (backupFile.available()) {
                backupContent += (char)backupFile.read();
            }
            backupFile.close();
            
            // Add backup content before current content
            result = backupContent + result;
        }
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
