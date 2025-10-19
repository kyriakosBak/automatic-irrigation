#include "logger.h"
#include <LittleFS.h>

// Log queue structure
struct LogEntry {
    char message[MAX_LOG_ENTRY_SIZE];
    unsigned long timestamp_millis;
    bool valid;
};

// Circular buffer for log queue
static LogEntry log_queue[LOG_QUEUE_SIZE];
static volatile int queue_write_index = 0;
static volatile int queue_read_index = 0;
static volatile int queue_count = 0;

// Mutex for thread safety (using a simple flag since ESP32 doesn't have true threading in Arduino)
static volatile bool log_mutex = false;

// Statistics
static unsigned long logs_dropped = 0;
static unsigned long logs_written = 0;

// Helper function to acquire mutex
static bool acquire_mutex(unsigned long timeout_ms = 100) {
    unsigned long start = millis();
    while (log_mutex) {
        if (millis() - start > timeout_ms) {
            return false;
        }
        delay(1);
    }
    log_mutex = true;
    return true;
}

// Helper function to release mutex
static void release_mutex() {
    log_mutex = false;
}

void logger_init() {
    // Initialize queue
    for (int i = 0; i < LOG_QUEUE_SIZE; i++) {
        log_queue[i].valid = false;
        log_queue[i].message[0] = '\0';
        log_queue[i].timestamp_millis = 0;
    }
    queue_write_index = 0;
    queue_read_index = 0;
    queue_count = 0;
    log_mutex = false;
    logs_dropped = 0;
    logs_written = 0;
    
    // Logger initialization
    Serial.println("Logger initialized with queue-based system");
    
    // Create initial log entry if LittleFS is available
    if (LittleFS.begin()) {
        logger_log("Logger system initialized with buffered writing");
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

// Get queue statistics
int logger_get_queue_count() {
    return queue_count;
}

unsigned long logger_get_dropped_count() {
    return logs_dropped;
}

unsigned long logger_get_written_count() {
    return logs_written;
}

void logger_log(const char* message) {
    if (!message || message[0] == '\0') {
        return; // Ignore empty messages
    }
    
    // Try to acquire mutex with short timeout to avoid blocking
    if (!acquire_mutex(50)) {
        // If we can't acquire mutex quickly, still queue without mutex
        // This is better than losing the log entirely
        Serial.println("LOG (no mutex): " + String(message));
    }
    
    // Check if queue is full
    if (queue_count >= LOG_QUEUE_SIZE) {
        logs_dropped++;
        Serial.println("LOG QUEUE FULL! Dropped: " + String(logs_dropped) + " - " + String(message));
        if (log_mutex) release_mutex();
        return;
    }
    
    // Add to queue
    LogEntry* entry = &log_queue[queue_write_index];
    strncpy(entry->message, message, MAX_LOG_ENTRY_SIZE - 1);
    entry->message[MAX_LOG_ENTRY_SIZE - 1] = '\0'; // Ensure null termination
    entry->timestamp_millis = millis();
    entry->valid = true;
    
    // Update queue indices
    queue_write_index = (queue_write_index + 1) % LOG_QUEUE_SIZE;
    queue_count++;
    
    // Also print to serial immediately for debugging
    Serial.println("LOG (queued): " + String(message));
    
    if (log_mutex) release_mutex();
}

void logger_process_queue() {
    // Process up to 10 log entries per call to avoid blocking too long
    int processed = 0;
    const int max_per_cycle = 10;
    
    while (processed < max_per_cycle && queue_count > 0) {
        if (!acquire_mutex(100)) {
            // Couldn't acquire mutex, try again next cycle
            return;
        }
        
        // Check if there are entries to process
        if (queue_count <= 0) {
            release_mutex();
            break;
        }
        
        // Get the next entry to process
        LogEntry* entry = &log_queue[queue_read_index];
        if (!entry->valid) {
            // Skip invalid entries
            queue_read_index = (queue_read_index + 1) % LOG_QUEUE_SIZE;
            queue_count--;
            release_mutex();
            continue;
        }
        
        // Copy entry data before releasing mutex
        char message_copy[MAX_LOG_ENTRY_SIZE];
        strncpy(message_copy, entry->message, MAX_LOG_ENTRY_SIZE);
        message_copy[MAX_LOG_ENTRY_SIZE - 1] = '\0';
        
        // Mark entry as processed
        entry->valid = false;
        queue_read_index = (queue_read_index + 1) % LOG_QUEUE_SIZE;
        queue_count--;
        
        release_mutex();
        
        // Now write to file (without holding mutex)
        // Check if rotation is needed first
        File logFile = LittleFS.open(LOG_FILE_PATH, "r");
        if (logFile) {
            size_t fileSize = logFile.size();
            logFile.close();
            
            if (fileSize >= MAX_LOG_FILE_SIZE) {
                // Rotate log file
                if (LittleFS.exists(LOG_FILE_BACKUP_PATH)) {
                    LittleFS.remove(LOG_FILE_BACKUP_PATH);
                }
                LittleFS.rename(LOG_FILE_PATH, LOG_FILE_BACKUP_PATH);
                Serial.println("Log file rotated");
            }
        }
        
        // Open file and append
        logFile = LittleFS.open(LOG_FILE_PATH, "a");
        if (!logFile) {
            Serial.println("Failed to open log file for writing: " + String(LOG_FILE_PATH));
            processed++;
            continue;
        }
        
        // Get timestamp
        String timestamp = get_timestamp();
        String logEntry = "[" + timestamp + "] " + String(message_copy) + "\n";
        
        // Write to file
        size_t written = logFile.print(logEntry);
        logFile.flush(); // Force write to filesystem
        logFile.close();
        
        if (written > 0) {
            logs_written++;
        }
        
        processed++;
    }
    
    // Periodically report statistics
    static unsigned long last_stats_report = 0;
    if (millis() - last_stats_report > 60000) { // Every 60 seconds
        if (logs_dropped > 0) {
            Serial.println("LOG STATS: Written=" + String(logs_written) + ", Dropped=" + String(logs_dropped) + ", Queued=" + String(queue_count));
        }
        last_stats_report = millis();
    }
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
