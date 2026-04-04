# 🔌 ESP32 Master Pin Configuration

| Component        | Component Pin | ESP32 Pin  | Notes |
|------------------|--------------|------------|------|
| All Sensors      | VCC / VIN    | 3V3        | Connect to the Red (+) rail on breadboard |
| All Sensors      | GND          | GND        | Connect to the Blue (-) rail on breadboard |
| I2C Bus          | SDA          | GPIO 21    | Shared by MPU6050, MAX30102, & MAX30205 |
| I2C Bus          | SCL          | GPIO 22    | Shared by MPU6050, MAX30102, & MAX30205 |
| FSR 402          | Pin 1        | 3V3        | Goes to the power rail |
| FSR 402          | Pin 2        | GPIO 32    | Also needs a 10k Resistor to GND (Voltage Divider) |
| Active Buzzer    | Positive (+) | GPIO 25    | Controlled by the "Panic" logic in your code |
| Active Buzzer    | Negative (-) | GND        | Goes to the ground rail |