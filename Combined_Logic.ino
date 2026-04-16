#include <Wire.h>
#include "MAX30105.h"

#include <WiFi.h> 
#include "ThingSpeak.h"
#include <Wire.h>
#include "MAX30105.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ===== WiFi & ThingSpeak Globals =====
const char* ssid = "AndroidAP8F02";        
const char* password = "zkaa3250"; 

unsigned long myChannelNumber = 3345010;          
const char * myWriteAPIKey = "40HM4DY5ZPH0BOY3";

WiFiClient client;
unsigned long lastThingSpeakUpdate = 0;
const unsigned long updateInterval = 20000; 

// ===== MAX30102 Globals =====
MAX30105 particleSensor;
long dcFilter = 0;
long lastBeat = 0;
int beatAvg = 75;
float smoothSpO2 = 98.0;
bool fingerDetected = false; 

// ===== MPU6050 Globals =====
Adafruit_MPU6050 mpu;

// ===== Hardware Pins =====
const int BUZZER_PIN = 25; 

// ===== Thresholds =====
const float IMPACT_THRESHOLD = 30.0;   //This will mostly be 3G
const float FREEFALL_THRESHOLD = 3.5;  
const float GYRO_THRESHOLD = 1.8;      
const int VERIFICATION_TIME = 2000;
const int TELEMETRY_INTERVAL = 2000;
// We need to remove this when we are adding a stop button 
const int ALERT_DURATION = 10000; // auto stop after 10s

//  Filtering (Just a Simple Low Pass Filter)
float alpha = 0.7;
float Acc = 0, prevAcc = 0;
float W = 0, prevW = 0;

// Variables For Detection Logic
unsigned long impactTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long alertStartTime = 0;

bool verifyingFall = false;
bool alertActive = false;
bool freeFallDetected = false;

float peakG = 0.0;

void setup()
{
  Serial.begin(115200);
  
  // Initialize shared I2C bus for both sensors
  Wire.begin(21, 22);

  // ===== MPU6050 Setup =====
  pinMode(BUZZER_PIN, OUTPUT);

  if (!mpu.begin()) {
    Serial.println("MPU6050 Error!, Check Connection V.1 ERROR");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("ALL Systems ARE UP AND RUNNING!");

  // ===== MAX30102 Setup =====
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Sensor not found!");
    while (1);
  }

  particleSensor.setup(60, 4, 2, 100, 411, 4096);

  // ===== WiFi Setup =====
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  
  ThingSpeak.begin(client);
}

void loop()
{
  // ==========================================
  //            MAX30102 LOGIC
  // ==========================================
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
    // Note: The original 'return;' was removed here so the MPU6050 logic still runs
  }
  else
  {
    if (!fingerDetected)
    {
      Serial.println("Finger detected");
      fingerDetected = true;
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

        Serial.print("SpO2: ");
        Serial.print(smoothSpO2, 1);
        Serial.print("% \t BPM: ");
        Serial.println(beatAvg);

        // ThingSpeak runs strictly within the MAX logic
        if (millis() - lastThingSpeakUpdate > updateInterval) 
        {
          ThingSpeak.setField(1, smoothSpO2);
          ThingSpeak.setField(2, beatAvg);
          
          int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
          
          if(x == 200){
            Serial.println("--> ThingSpeak Update Successful!");
          } else {
            Serial.println("--> ThingSpeak Update Failed. HTTP error code " + String(x));
          }
          
          lastThingSpeakUpdate = millis();
        }
      }
    }
  }

  // ==========================================
  //            MPU6050 LOGIC
  // ==========================================
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // This is the Acceleration Magnitude (Vector Sum) and Gyro Magnitude (Vector Sum)
  float rawAcc = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));
  float rawW   = sqrt(sq(g.gyro.x) + sq(g.gyro.y) + sq(g.gyro.z));

  // ===== FILTER =====
  Acc = alpha * rawAcc + (1 - alpha) * prevAcc;
  W   = alpha * rawW   + (1 - alpha) * prevW;

  prevAcc = Acc;
  prevW = W;

  float currentG = Acc / 9.806;

  //Telemetry System (for debugging and tuning)
  if (millis() - lastTelemetryTime > TELEMETRY_INTERVAL) {
    Serial.print("G Force :  "); Serial.print(currentG);
    Serial.print(" Rotational Velocity: "); Serial.println(W);
    lastTelemetryTime = millis();
  }

  // Fall Detection Logic
  if (!alertActive) {

    if (Acc < FREEFALL_THRESHOLD) {
      freeFallDetected = true;
    }

    if (Acc > IMPACT_THRESHOLD && !verifyingFall) {
      Serial.println("IMPACT was detected! V1.0");
      impactTime = millis();
      verifyingFall = true;
      peakG = currentG;
    }

    if (verifyingFall) {

      float tilt = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
      bool isHorizontal = abs(tilt) > 30;

      if (millis() - impactTime > VERIFICATION_TIME) {

        bool fallDetected = false;

        if (isHorizontal || W > GYRO_THRESHOLD) {
          fallDetected = true;
        }

        if (freeFallDetected && peakG > 2.5) {
          fallDetected = true;
        }

        if (fallDetected) {
          triggerAlert();
        } else {
          Serial.println("===Not a fall===");
        }

        verifyingFall = false;
        freeFallDetected = false;
      }
    }

  } else {
    handleEmergencyState();
  }

  delay(20);
}

// Alert Function
void triggerAlert() {
  alertActive = true;
  alertStartTime = millis();

  Serial.print("FALL CONFIRMED! Peak: ");
  Serial.print(peakG);
  Serial.println(" G");

  digitalWrite(BUZZER_PIN, HIGH);
}

// Emergency State Function (Buzzer Pulsing and Auto Stop)
void handleEmergencyState() {

  // Auto stop after some time UNTILL WE CONNECT A stop button
  if (millis() - alertStartTime > ALERT_DURATION) {
    Serial.println("<======= Alert auto-stopped");
    alertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  // Pulsing buzzer
  digitalWrite(BUZZER_PIN, (millis() % 300 < 150) ? HIGH : LOW);
}

//PROTOTYPE V1.0