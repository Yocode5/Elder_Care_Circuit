#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// ===== Hardware Pins =====
const int BUZZER_PIN = 25; 

// ===== Thresholds =====
const float IMPACT_THRESHOLD = 30.0;
const float FREEFALL_THRESHOLD = 3.5;
const float GYRO_THRESHOLD = 1.8;
const int VERIFICATION_TIME = 2000;
const int TELEMETRY_INTERVAL = 100;

// Alert auto stop
const int ALERT_DURATION = 10000;

// ===== Filtering =====
float alpha = 0.7;
float Acc = 0, prevAcc = 0;
float W = 0, prevW = 0;

// ===== Variables =====
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
    Serial.println("MPU6050 ERROR");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("SYSTEM READY");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // ===== RAW VALUES =====
  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float gx = g.gyro.x * 57.2958; // convert rad/s → deg/s
  float gy = g.gyro.y * 57.2958;
  float gz = g.gyro.z * 57.2958;

  // ===== Magnitudes =====
  float rawAcc = sqrt(ax * ax + ay * ay + az * az);
  float rawW   = sqrt(gx * gx + gy * gy + gz * gz);

  // ===== FILTER =====
  Acc = alpha * rawAcc + (1 - alpha) * prevAcc;
  W   = alpha * rawW   + (1 - alpha) * prevW;

  prevAcc = Acc;
  prevW = W;

  float currentG = Acc / 9.806;

  // ===== FALL DETECTION =====
  if (!alertActive) {

    if (Acc < FREEFALL_THRESHOLD) {
      freeFallDetected = true;
    }

    if (Acc > IMPACT_THRESHOLD && !verifyingFall) {
      impactTime = millis();
      verifyingFall = true;
      peakG = currentG;
    }

    if (verifyingFall) {

      float tilt = atan2(ay, az) * 180 / PI;
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
        }

        verifyingFall = false;
        freeFallDetected = false;
      }
    }

  } else {
    handleEmergencyState();
  }

  // ===== SEND DATA TO UNITY =====
  if (millis() - lastTelemetryTime > TELEMETRY_INTERVAL) {

    Serial.print(ax); Serial.print(",");
    Serial.print(ay); Serial.print(",");
    Serial.print(az); Serial.print(",");

    Serial.print(gx); Serial.print(",");
    Serial.print(gy); Serial.print(",");
    Serial.print(gz); Serial.print(",");

    Serial.print(currentG); Serial.print(",");
    Serial.println(alertActive ? 1 : 0);

    lastTelemetryTime = millis();
  }

  delay(10);
}

// ===== ALERT =====
void triggerAlert() {
  alertActive = true;
  alertStartTime = millis();

  digitalWrite(BUZZER_PIN, HIGH);

  Serial.println("FALL CONFIRMED");
}

// ===== EMERGENCY =====
void handleEmergencyState() {

  if (millis() - alertStartTime > ALERT_DURATION) {
    alertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  // buzzer pulse
  digitalWrite(BUZZER_PIN, (millis() % 300 < 150) ? HIGH : LOW);
}