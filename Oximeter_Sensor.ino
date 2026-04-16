#include <Wire.h>
#include "MAX30105.h"

MAX30105 particleSensor;

long dcFilter = 0;
long lastBeat = 0;
int beatAvg = 75;
float smoothSpO2 = 98.0;

bool fingerDetected = false; 

void setup()
{
  Serial.begin(115200);
  
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Sensor not found!");
    while (1);
  }

  particleSensor.setup(60, 4, 2, 100, 411, 4096);
}

void loop()
{
  long ir = particleSensor.getIR();
  long red = particleSensor.getRed();
  
  if (ir < 50000) 
  {
    if (fingerDetected) 
    {
      Serial.println("Finger not detected");
      fingerDetected = false;
    }

    dcFilter = 0;
    return;
  }
  else
  {
    if (!fingerDetected)
    {
      Serial.println("Finger detected");
      fingerDetected = true;
    }
  }

  if (dcFilter == 0) dcFilter = ir;
  dcFilter = (dcFilter * 0.85) + (ir * 0.15); 
  long acPulse = ir - dcFilter;

  float R = (float)red / (float)ir;
  float spo2 = 110.0 - 16.0 * R;

  spo2 = constrain(spo2, 90.0, 100.0);

  smoothSpO2 = (smoothSpO2 * 0.85) + (spo2 * 0.15);

  if (acPulse > 60 && (millis() - lastBeat > 400)) 
  {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    float bpm = 60000.0 / delta;

    if (bpm > 50 && bpm < 120)
    {
      beatAvg = (beatAvg * 0.7) + (bpm * 0.3);

      // -------- OUTPUT --------
      Serial.print("SpO2: ");
      Serial.print(smoothSpO2, 1);
      Serial.print("% \t BPM: ");
      Serial.println(beatAvg);
    }
  }
}