#include <Wire.h>

// I2C Address is permanently 0x48 because A0, A1, and A2 are grounded
const int MAX30205_ADDRESS = 0x48; 

void setup() {
  // Start Serial Monitor at 115200 baud
  Serial.begin(115200);
  
  // Start I2C using standard ESP32 pins (SDA = 21, SCL = 22)
  Wire.begin(); 
  
  Serial.println("=====================================");
  Serial.println("  MAX30205 Body Temperature Monitor  ");
  Serial.println("=====================================");
  delay(1000);
}

void loop() {
  float tempCelsius = readBodyTemp();

  // If the function returns anything other than -1.0, it's a good reading
  if (tempCelsius != -1.0) {
    Serial.print("Body Temp: ");
    Serial.print(tempCelsius, 2); // Print with 2 decimal places
    Serial.println(" °C");
  } else {
    Serial.println("ERROR: I2C line disconnected or sensor unresponsive.");
  }

  // Wait 1 second before taking the next reading
  delay(1000); 
}

float readBodyTemp() {
  Wire.beginTransmission(MAX30205_ADDRESS);
  Wire.write(0x00);
  byte error = Wire.endTransmission();

  if (error != 0) {
    return -1.0; 
  }

  Wire.requestFrom(MAX30205_ADDRESS, 2);

  if (Wire.available() == 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    int16_t rawTemp = (msb << 8) | lsb;
    
    return rawTemp * 0.00390625; 
  }

  return -1.0; 
}