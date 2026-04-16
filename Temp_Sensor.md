# ESP32 & MAX30205 Body Temperature Monitor

A high-precision, I2C-based human body temperature monitoring system using the ESP32 microcontroller and the MAX30205 clinical-grade temperature sensor.

---

## Pin Configuration (Wiring)

The MAX30205 communicates via I2C. Wire it to the ESP32 exactly as shown below:

| MAX30205 Pin | ESP32 Pin | Description |
|-------------|----------|------------|
| VCC         | 3V3      | 3.3V Power Supply (**Do NOT connect to 5V**) |
| GND         | GND      | Common Ground |
| SDA         | GPIO 21  | Standard I2C Data Line |
| SCL         | GPIO 22  | Standard I2C Clock Line |
| A0, A1, A2  | GND      | Hardware Address Pins *(See warning below)* |
| OS          | Unconnected | Overtemperature Shutdown *(Not used in this setup)* |

---

## Notes

- The ESP32 uses **GPIO 21 (SDA)** and **GPIO 22 (SCL)** as default I2C pins.
- Ensure all grounds are connected properly.
- Never power the MAX30205 with 5V — it can damage the sensor.