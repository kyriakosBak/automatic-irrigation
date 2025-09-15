#!/bin/bash

# OTA Update Script for Automatic Irrigation System
# This script helps you update your ESP32 wirelessly

echo "🌱 Automatic Irrigation System - OTA Updater"
echo "=============================================="

# Check if ESP32 is reachable
echo "🔍 Looking for ESP32..."
if ping -c 1 irrigation-system.local > /dev/null 2>&1; then
    echo "✅ ESP32 found at irrigation-system.local"
    ESP32_IP="irrigation-system.local"
elif ping -c 1 irrigation-system > /dev/null 2>&1; then
    echo "✅ ESP32 found at irrigation-system"
    ESP32_IP="irrigation-system"
else
    echo "❌ ESP32 not found. Please check:"
    echo "   1. ESP32 is powered on and connected to WiFi"
    echo "   2. You're on the same network"
    echo "   3. mDNS is working (try finding the IP manually)"
    read -p "Enter ESP32 IP address manually (or press Enter to exit): " MANUAL_IP
    if [ -z "$MANUAL_IP" ]; then
        echo "Exiting..."
        exit 1
    fi
    ESP32_IP="$MANUAL_IP"
fi

# Get device status
echo "📊 Getting device status..."
curl -s "http://$ESP32_IP/api/status" | jq '.' 2>/dev/null || echo "Could not get status (device may not be ready)"

echo ""
echo "🚀 Starting OTA update..."
echo "Password: irrigation2024"
echo ""

# Build and upload via OTA
platformio run --environment esp32dev-ota --target upload

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ OTA Update completed successfully!"
    echo "🔄 ESP32 is rebooting..."
    sleep 5
    echo "📊 Checking if device is back online..."
    
    # Wait for device to come back online
    for i in {1..30}; do
        if curl -s "http://$ESP32_IP/api/status" > /dev/null 2>&1; then
            echo "✅ Device is back online!"
            curl -s "http://$ESP32_IP/api/status" | jq '.'
            break
        fi
        echo "⏳ Waiting for device... ($i/30)"
        sleep 2
    done
else
    echo ""
    echo "❌ OTA Update failed!"
    echo "💡 Troubleshooting tips:"
    echo "   1. Make sure ESP32 is not running pumps/watering sequence"
    echo "   2. Check WiFi connection"
    echo "   3. Verify password is correct"
    echo "   4. Try uploading via USB if OTA continues to fail"
fi
