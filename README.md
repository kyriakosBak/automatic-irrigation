# 🌱 Automatic Irrigation & Fertilizer Dosing System

A sophisticated ESP32-based automatic irrigation system that provides precise liquid fertilizer dosing, scheduling, and remote monitoring capabilities. This system automates plant care with intelligent pump control, sensor feedback, and a comprehensive web interface.

> **Note**: This project is 99% created with vibe coding 🎵✨

## 🚀 Features

### Core Functionality
- **Automated Fertilizer Dosing**: Precise control of 5 different liquid fertilizers with configurable dosing amounts per day of the week
- **Scheduled Watering**: Daily scheduling with customizable timing and duration
- **Multi-Pump System**: Independent control of fertilizer pumps, watering pump, and humidifier pump
- **Smart Tank Management**: Automated main tank filling with liquid level sensing and safety timeouts
- **State Machine Control**: Robust watering sequence management (IDLE → DOSING → FILLING → FILLED → WATERING)

### Web Interface & Monitoring
- **Real-time Status Dashboard**: Monitor pump states, tank levels, and system health
- **Weekly Schedule Management**: Configure different fertilizer dosing amounts for each day
- **Manual Override Controls**: Debug and test individual pumps and valves
- **Comprehensive Logging**: Activity logs with download capability
- **RESTful API**: Complete API for integration with external systems

### Connectivity & Updates
- **WiFi Management**: Automatic connection with AP fallback mode for initial setup
- **Over-the-Air (OTA) Updates**: Wireless firmware updates with password protection
- **NTP Time Synchronization**: Accurate scheduling with timezone support (CET/CEST)
- **mDNS Support**: Easy device discovery as `irrigation-system.local`

### Safety & Reliability
- **Automatic Safety Shutdowns**: Pumps stop during OTA updates and on errors
- **Configurable Timeouts**: Protection against stuck valves and runaway pumps
- **Sensor Integration**: Capacitive liquid level sensor prevents overflow
- **Persistent Settings**: Configuration stored in NVS (Non-Volatile Storage)

## 🔧 Hardware Components

### Core Components
- **ESP32 Development Board**: Main microcontroller
- **PCA9685 PWM Driver Boards**: Motor control (expandable for more pumps)
- **Peristaltic Pumps (7x)**:
  - 5× Fertilizer pumps (Bio-Grow, Bio-Bloom, Top-Max, CalMag, PhDown)
  - 1× Main watering pump
  - 1× Humidifier tank pump
- **Solenoid Valve**: Main tank filling control
- **Capacitive Liquid Level Sensor**: Tank level detection

### Connections
```
ESP32 → PCA9685 Motor Shield
Pin 22 (SCL) → SCL
Pin 21 (SDA) → SDA
Pin 32 → Liquid Level Sensor
Pin 13 → Solenoid Valve
```

## 📁 Project Structure

```
automatic-irrigation/
├── src/
│   ├── main.cpp              # Main application logic & web server
│   ├── config/
│   │   └── config.h          # Hardware configuration & constants
│   └── modules/              # Hardware abstraction layer
│       ├── motor_shield_control.{cpp,h}  # PCA9685 motor control
│       ├── pump_control.{cpp,h}          # Pump management & dosing
│       ├── valve_control.{cpp,h}         # Solenoid valve control
│       ├── scheduler.{cpp,h}             # Time-based scheduling
│       ├── sensors.{cpp,h}               # Sensor reading
│       └── logger.{cpp,h}                # System logging
├── data/
│   ├── index.html            # Web interface (1500+ lines)
│   └── wifi.json             # WiFi credentials storage
├── platformio.ini            # PlatformIO configuration
├── update_ota.sh             # OTA update helper script
└── OTA_GUIDE.md             # Comprehensive OTA documentation
```

## 🛠️ Development Environment

This project uses **PlatformIO** for development with support for multiple ESP32 variants:

### Supported Boards
- `esp32dev`: Standard ESP32 development board
- `d1_mini`: Wemos D1 Mini ESP32

### Dependencies
```ini
lib_deps = 
    adafruit/Adafruit Motor Shield V2 Library@^1.1.3
    Wire
    ESPAsyncWebServer
    ArduinoJson@^6.21.3
    ArduinoOTA
```

## 🚦 Getting Started

### 1. Hardware Setup
1. Connect ESP32 to PCA9685 motor shield via I2C (pins 21, 22)
2. Connect pumps to motor shield outputs (motors 1-7)
3. Connect solenoid valve to GPIO pin 13
4. Connect liquid level sensor to GPIO pin 32
5. Power system with appropriate voltage for pumps

### 2. Initial Firmware Upload
```bash
# First upload via USB
platformio run --environment esp32dev --target upload

# Upload web interface files
platformio run --target uploadfs
```

### 3. WiFi Configuration
1. On first boot, device creates "IrrigationSetup" AP
2. Connect to AP and navigate to device IP
3. Access `/wifi` endpoint to configure WiFi credentials
4. System will reboot and connect to your network

### 4. Access Web Interface
- **Local Network**: `http://irrigation-system.local`
- **Direct IP**: Check serial output or router for assigned IP
- **Features**: Schedule configuration, manual controls, system monitoring

## 🔄 Over-the-Air Updates

### Quick Update
```bash
./update_ota.sh
```

### Manual OTA
```bash
platformio run --environment esp32dev-ota --target upload
```

**OTA Credentials**:
- Hostname: `irrigation-system`
- Password: `irrigation2024`

## 📊 API Endpoints

### System Control
- `POST /api/start_watering` - Trigger watering sequence
- `POST /api/stop_all_pumps` - Emergency stop all pumps
- `GET /api/status` - System status and sensor readings

### Schedule Management
- `GET/POST /api/weekly_dosing` - Fertilizer dosing schedule
- `GET/POST /api/weekly_watering_enabled` - Enable/disable watering by day
- `GET/POST /api/schedule` - Daily watering time

### Pump Control
- `POST /api/debug_pump` - Manual pump control
- `GET/POST /api/calibration` - Pump calibration values
- `GET/POST /api/fertilizer_motor_speed` - Motor speed settings

### Monitoring
- `GET /api/logs` - System activity logs
- `DELETE /api/logs` - Clear logs
- `GET /api/ota_info` - OTA update information

## ⚙️ Configuration

### Fertilizer Dosing
Configure weekly dosing amounts via web interface:
- **Bio-Grow**: Vegetative growth nutrients
- **Bio-Bloom**: Flowering nutrients  
- **Top-Max**: Bloom enhancer
- **CalMag**: Calcium and Magnesium supplement
- **PhDown**: pH adjustment

### Scheduling
- **Daily Time**: Set hour and minute for automatic watering
- **Day Enable/Disable**: Control which days watering occurs
- **Duration**: Configurable watering duration (1-30 minutes)

### Safety Settings
- **Fill Timeout**: Maximum time for tank filling (default: 60 seconds)
- **Max Watering**: Maximum watering duration (default: 5 minutes)
- **Pump Calibration**: ml/sec calibration for accurate dosing

## 🔧 Troubleshooting

### Common Issues
1. **WiFi Connection**: Check credentials, restart device, or use AP mode
2. **Pump Not Running**: Verify motor shield connections and power supply  
3. **Dosing Inaccurate**: Calibrate pumps via web interface
4. **OTA Fails**: Ensure stable WiFi and correct password

### Debug Features
- **Serial Monitor**: 115200 baud for diagnostic output
- **Manual Pump Control**: Test individual components via web interface
- **System Logs**: Comprehensive logging with timestamps
- **API Testing**: Use curl or Postman to test API endpoints

## 🤝 Contributing

This system is designed with modularity in mind. Key areas for enhancement:
- Additional sensor integration
- Mobile app development
- Database logging integration
- Advanced scheduling algorithms
- Multi-zone expansion

## 📄 License

This project is 99% created with vibe coding ✨ - built with passion for automated growing systems.

## 🔗 Related Documentation

- [OTA Update Guide](OTA_GUIDE.md) - Comprehensive wireless update instructions
- [Hardware Wiring Diagrams](docs/wiring/) - Detailed connection guides  
- [API Documentation](docs/api/) - Complete REST API reference

---

**Happy Growing! 🌿🤖**