#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// ===== Hardware Pins =====
const int BUZZER_PIN = 25; 

// ===== Thresholds =====
const float IMPACT_THRESHOLD = 30.0;   // ~3G
const float FREEFALL_THRESHOLD = 5.5;  
const float GYRO_THRESHOLD = 1.8;      
const int VERIFICATION_TIME = 2000;
const int TELEMETRY_INTERVAL = 2000;
const int ALERT_DURATION = 10000; // auto stop after 10s

// ===== Filtering =====
float alpha = 0.7;
float Acc = 0, prevAcc = 0;
float W = 0, prevW = 0;

// ===== State Variables =====
unsigned long impactTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long alertStartTime = 0;

bool verifyingFall = false;
bool alertActive = false;
bool freeFallDetected = false;

float peakG = 0.0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  pinMode(BUZZER_PIN, OUTPUT);

  if (!mpu.begin()) {
    Serial.println("MPU6050 Error!");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("ElderGuard (No FSR): Fall Detection Active 🚀");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // ===== RAW VALUES =====
  float rawAcc = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));
  float rawW   = sqrt(sq(g.gyro.x) + sq(g.gyro.y) + sq(g.gyro.z));

  // ===== FILTER =====
  Acc = alpha * rawAcc + (1 - alpha) * prevAcc;
  W   = alpha * rawW   + (1 - alpha) * prevW;

  prevAcc = Acc;
  prevW = W;

  float currentG = Acc / 9.806;

  // ===== TELEMETRY =====
  if (millis() - lastTelemetryTime > TELEMETRY_INTERVAL) {
    Serial.print("📡 G: "); Serial.print(currentG);
    Serial.print(" | Rot: "); Serial.println(W);
    lastTelemetryTime = millis();
  }

  // ===== DETECTION =====
  if (!alertActive) {

    if (Acc < FREEFALL_THRESHOLD) {
      freeFallDetected = true;
    }

    if (Acc > IMPACT_THRESHOLD && !verifyingFall) {
      Serial.println("💥 IMPACT detected!");
      impactTime = millis();
      verifyingFall = true;
      peakG = currentG;
    }

    if (verifyingFall) {

      float tilt = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
      bool isHorizontal = abs(tilt) > 30;

      if (millis() - impactTime > VERIFICATION_TIME) {

        bool fallDetected = false;

        if (isHorizontal && W > GYRO_THRESHOLD) {
          fallDetected = true;
        }

        if (freeFallDetected && peakG > 2.5) {
          fallDetected = true;
        }

        if (fallDetected) {
          triggerAlert();
        } else {
          Serial.println("❌ Not a fall");
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

// ===== ALERT =====
void triggerAlert() {
  alertActive = true;
  alertStartTime = millis();

  Serial.print("🚨 FALL CONFIRMED! Peak: ");
  Serial.print(peakG);
  Serial.println(" G");

  digitalWrite(BUZZER_PIN, HIGH);
}

// ===== EMERGENCY MODE =====
void handleEmergencyState() {

  // Auto stop after some time (since no FSR)
  if (millis() - alertStartTime > ALERT_DURATION) {
    Serial.println("⏹️ Alert auto-stopped");
    alertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  // Pulsing buzzer
  digitalWrite(BUZZER_PIN, (millis() % 300 < 150) ? HIGH : LOW);
}