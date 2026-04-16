# 📌 ESP32 ↔ MAX30102 Pin Configuration

## 🔧 Wiring Overview

| MAX30102 Pin | ESP32 Pin | Description |
|-------------|----------|-------------|
| VIN / VCC   | 3.3V     | Power supply (⚠️ DO NOT use 5V) |
| GND         | GND      | Ground connection |
| SDA         | GPIO 21  | I2C Data line |
| SCL         | GPIO 22  | I2C Clock line |
| INT (optional) | Not connected | Interrupt pin (not needed for this project) |