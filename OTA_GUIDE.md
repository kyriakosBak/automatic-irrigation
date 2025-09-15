# OTA (Over-The-Air) Updates Guide

Your ESP32 irrigation system now supports wireless code updates! This means you can update the firmware without physically accessing the device.

## 🔧 Setup Complete

The following OTA features have been added to your system:

- **Arduino OTA**: Built into the firmware for wireless updates
- **Password Protection**: Updates require the password `irrigation2024`
- **Hostname**: Device is accessible as `irrigation-system.local`
- **Safety Features**: All pumps/valves stop during OTA updates
- **Status Monitoring**: OTA progress is logged to the system

## 🚀 How to Use OTA Updates

### Method 1: Using the OTA Script (Recommended)
```bash
./update_ota.sh
```

This script will:
1. Check if your ESP32 is online
2. Display current device status
3. Upload the new firmware wirelessly
4. Wait for device to reboot and verify it's working

### Method 2: Manual PlatformIO OTA
```bash
# Build and upload via OTA
platformio run --environment esp32dev-ota --target upload
```

### Method 3: Using Arduino IDE OTA
If you prefer Arduino IDE:
1. Go to Tools → Port
2. Select "irrigation-system at xxx.xxx.xxx.xxx (ESP32 Dev Module)"
3. Upload normally

## 📡 Network Requirements

- ESP32 and your computer must be on the same network
- ESP32 must have a stable WiFi connection
- mDNS should be working (usually automatic)
- Port 3232 should be accessible for OTA

## 🔐 Security

- **Password**: `irrigation2024`
- **Change Password**: Edit the line in `main.cpp`:
  ```cpp
  ArduinoOTA.setPassword("your-new-password");
  ```
- **Disable OTA**: Comment out the `ArduinoOTA.begin()` line

## 🛠️ Troubleshooting

### ESP32 Not Found
1. Check ESP32 is powered and connected to WiFi
2. Try using IP address instead of hostname
3. Check your network allows device discovery

### OTA Upload Fails
1. Make sure no watering sequence is running
2. Verify password is correct
3. Check WiFi signal strength
4. Try restarting the ESP32

### Emergency Recovery
If OTA completely fails:
1. Use USB cable to upload: `platformio run --environment esp32dev --target upload`
2. The OTA functionality will be restored

## 🌐 Web Interface

Your irrigation system also provides OTA information via the web API:

- **Device Status**: `http://irrigation-system.local/api/status`
- **OTA Info**: `http://irrigation-system.local/api/ota_info`

## 📝 Configuration Files

### platformio.ini Environments

1. **esp32dev**: Standard USB upload (for initial setup)
2. **esp32dev-ota**: Wireless OTA upload

### OTA Settings
- **Hostname**: `irrigation-system`
- **Password**: `irrigation2024` 
- **Port**: 3232 (default Arduino OTA port)

## 🔄 Update Process

1. **Preparation**: System stops all pumps and valves
2. **Upload**: New firmware is transferred wirelessly  
3. **Installation**: ESP32 reboots with new firmware
4. **Verification**: System comes back online

## 💡 Tips

- **Backup**: Always test locally before OTA updates
- **Timing**: Avoid OTA during scheduled watering times
- **Monitoring**: Check logs for OTA status and errors
- **Recovery**: Keep USB cable handy as backup method

## 📊 Monitoring OTA Status

The system logs all OTA activities. Check via:
- Web interface: `/api/logs`
- Serial monitor during upload
- Status API for connectivity confirmation

---

**Happy wireless updating! 🌱**
