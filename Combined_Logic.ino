#include <Wire.h>
#include "MAX30105.h"

#include <WiFi.h> 
#include "ThingSpeak.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ===== WiFi & ThingSpeak =====
const char* ssid = "AndroidAP8F02";        
const char* password = "zkaa3250"; 

unsigned long myChannelNumber = 3345010;          
const char * myWriteAPIKey = "40HM4DY5ZPH0BOY3";

WiFiClient client;
unsigned long lastThingSpeakUpdate = 0;
const unsigned long updateInterval = 20000; 

// ===== FIREBASE =====
#define API_KEY "AIzaSyB84bwuO6LXTUUMasOKdjyBsWtBAvdhLQo"
#define FIREBASE_PROJECT_ID "elder-care-monitoring-db" 
#define DEVICE_ID "7MOLDH3H3"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===== MAX30102 =====
MAX30105 particleSensor;
long dcFilter = 0;
long lastBeat = 0;
int beatAvg = 75;
float smoothSpO2 = 98.0;
bool fingerDetected = false; 

// ===== MPU6050 =====
Adafruit_MPU6050 mpu;

// ===== PINS =====
const int BUZZER_PIN = 25; 
const int FSR_PIN = 34;

// ===== THRESHOLDS (UNCHANGED) =====
const float IMPACT_THRESHOLD = 30.0;
const float FREEFALL_THRESHOLD = 3.5;
const float GYRO_THRESHOLD = 1.8;
const int VERIFICATION_TIME = 2000;
const int TELEMETRY_INTERVAL = 2000;
const int ALERT_DURATION = 60000;

// ===== FILTER =====
float alpha = 0.7;
float Acc = 0, prevAcc = 0;
float W = 0, prevW = 0;

// ===== VARIABLES =====
unsigned long impactTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long alertStartTime = 0;

bool verifyingFall = false;
bool alertActive = false;
bool freeFallDetected = false;
bool emergencyTriggered = false;

float peakG = 0.0;

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FSR_PIN, INPUT);

  if (!mpu.begin()) {
    Serial.println("MPU6050 Error!, Check Connection V.1 ERROR");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("ALL Systems ARE UP AND RUNNING!");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Sensor not found!");
    while (1);
  }

  particleSensor.setup(60, 4, 2, 100, 411, 4096);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  
  ThingSpeak.begin(client);

  config.api_key = API_KEY;
  auth.user.email = "device@elderguard.com";
  auth.user.password = "123456";
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop()
{
  // ===== MAX30102 =====
  long ir = particleSensor.getIR();
  long red = particleSensor.getRed();
  
  if (ir < 50000) 
  {
    if (fingerDetected) {
      Serial.println("Finger not detected");
      fingerDetected = false;
    }
    dcFilter = 0;
  }
  else
  {
    if (!fingerDetected) {
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

        if (millis() - lastThingSpeakUpdate > updateInterval) 
        {
          ThingSpeak.setField(1, smoothSpO2);
          ThingSpeak.setField(2, beatAvg);
          ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
          lastThingSpeakUpdate = millis();
        }
      }
    }
  }

  // ===== MPU =====
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float rawAcc = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));
  float rawW   = sqrt(sq(g.gyro.x) + sq(g.gyro.y) + sq(g.gyro.z));

  Acc = alpha * rawAcc + (1 - alpha) * prevAcc;
  W   = alpha * rawW   + (1 - alpha) * prevW;

  prevAcc = Acc;
  prevW = W;

  float currentG = Acc / 9.806;

  if (millis() - lastTelemetryTime > TELEMETRY_INTERVAL) {
    Serial.print("G Force :  "); Serial.print(currentG);
    Serial.print(" Rotational Velocity: "); Serial.println(W);
    lastTelemetryTime = millis();
  }

  if (!alertActive) {

    if (Acc < FREEFALL_THRESHOLD) freeFallDetected = true;

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

        if (isHorizontal || W > GYRO_THRESHOLD) fallDetected = true;
        if (freeFallDetected && peakG > 2.5) fallDetected = true;

        if (fallDetected) triggerAlert();
        else Serial.println("===Not a fall===");

        verifyingFall = false;
        freeFallDetected = false;
      }
    }

  } else {

    // ===== FSR =====
    int fsrValue = 0;
    for (int i = 0; i < 5; i++) {
      fsrValue += analogRead(FSR_PIN);
      delay(2);
    }
    fsrValue /= 5;

    if (fsrValue > 200) {
      delay(50);
      if (analogRead(FSR_PIN) > 200) {

        alertActive = false;
        emergencyTriggered = false;
        digitalWrite(BUZZER_PIN, LOW);

        // ✅ RESOLVED INCIDENT
        FirebaseJson incident;
        incident.set("fields/peakG/doubleValue", peakG);
        incident.set("fields/status/stringValue", "RESOLVED");
        incident.set("fields/bpmAtTime/integerValue", beatAvg);
        incident.set("fields/spo2AtTime/doubleValue", smoothSpO2);
        incident.set("fields/timestamp/integerValue", millis());

        String path = "devices/" + String(DEVICE_ID) + "/incidents";
        Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), incident.raw());

        // ✅ BACK TO STABLE
        FirebaseJson status;
        status.set("fields/live_status/mapValue/fields/currentSituation/stringValue", "STABLE");

        String devicePath = "devices/" + String(DEVICE_ID);
        Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), status.raw(), "live_status");

        return;
      }
    }

    // ===== TIMEOUT → EMERGENCY =====
    if (!emergencyTriggered && millis() - alertStartTime > ALERT_DURATION) {

      FirebaseJson status;
      status.set("fields/live_status/mapValue/fields/currentSituation/stringValue", "UNRESPONSIVE");

      String devicePath = "devices/" + String(DEVICE_ID);
      Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), status.raw(), "live_status");

      FirebaseJson incident;
      incident.set("fields/peakG/doubleValue", peakG);
      incident.set("fields/status/stringValue", "EMERGENCY");
      incident.set("fields/bpmAtTime/integerValue", beatAvg);
      incident.set("fields/spo2AtTime/doubleValue", smoothSpO2);
      incident.set("fields/timestamp/integerValue", millis());

      String path = "devices/" + String(DEVICE_ID) + "/incidents";
      Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", path.c_str(), incident.raw());

      emergencyTriggered = true;
    }

    // 🔥 CANCEL EMERGENCY USING FSR (Reomve Once the modal was made)
    if (emergencyTriggered) {

      int fsrValue = 0;
      for (int i = 0; i < 5; i++) {
        fsrValue += analogRead(FSR_PIN);
        delay(2);
      }
      fsrValue /= 5;

      if (fsrValue > 200) {
        delay(50);
        if (analogRead(FSR_PIN) > 200) {

          emergencyTriggered = false;
          alertActive = false;

          digitalWrite(BUZZER_PIN, LOW);

          FirebaseJson status;
          status.set("fields/live_status/mapValue/fields/currentSituation/stringValue", "STABLE");

          String devicePath = "devices/" + String(DEVICE_ID);
          Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), status.raw(), "live_status");

          return;
        }
      }
    }

    handleEmergencyState();
  }

  delay(20);
}

void triggerAlert() {
  alertActive = true;
  alertStartTime = millis();
  emergencyTriggered = false;

  Serial.print("FALL CONFIRMED! Peak: ");
  Serial.print(peakG);
  Serial.println(" G");

  FirebaseJson status;
  status.set("fields/live_status/mapValue/fields/currentSituation/stringValue", "PENDING_RESPONSE");

  String devicePath = "devices/" + String(DEVICE_ID);
  Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", devicePath.c_str(), status.raw(), "live_status");

  digitalWrite(BUZZER_PIN, HIGH);
}

void handleEmergencyState() {
  digitalWrite(BUZZER_PIN, (millis() % 300 < 150) ? HIGH : LOW);
}

//Prototype V 1.0