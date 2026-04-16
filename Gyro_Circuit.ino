#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <HTTPClient.h>
#include <WiFi.h>

const char* ssid = "SLT-Fiber-2.4G0206";
const char* password = "haeL6342";
String apikey = "F9UFCGCW71GLSM5S";

Adafruit_MPU6050 mpu;

// ===== Hardware Pins =====
const int BUZZER_PIN = 25; 

// ===== Thresholds =====
const float IMPACT_THRESHOLD = 30.0;
const float FREEFALL_THRESHOLD = 3.5;  
const float GYRO_THRESHOLD = 1.8;      
const int VERIFICATION_TIME = 2000;
const int TELEMETRY_INTERVAL = 2000;
const int ALERT_DURATION = 10000;

// ===== ThingSpeak Timing (FIXED) =====
unsigned long lastSendTime = 0;
const int SEND_INTERVAL = 15000;

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

// ===== ThingSpeak Function =====
void sendToThingSpeak(float gValue , int fallFlag){
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;

    String url = "https://api.thingspeak.com/update?api_key=" + apikey +
                 "&field1=" + String(gValue) +
                 "&field2=" + String(fallFlag);

    http.begin(url);
    int httpCode = http.GET();

    Serial.println("ThingSpeak Res : ");
    Serial.println(httpCode);

    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n WiFi Connected! Ready for ACTION :)");

  Wire.begin(21, 22);
  pinMode(BUZZER_PIN, OUTPUT);

  if (!mpu.begin()) {
    Serial.println("MPU6050 Error!, Check Connection V.1 ERROR");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("ALL Systems ARE UP AND RUNNING!");
}

void loop() {
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

  // ===== Periodic ThingSpeak Update (FIXED) =====
  if (millis() - lastSendTime > SEND_INTERVAL && !alertActive) {
    sendToThingSpeak(currentG, 0);
    lastSendTime = millis();
  }

  // ===== Fall Detection =====
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
          sendToThingSpeak(peakG , 1); // 🚨 instant send
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

// ===== Alert =====
void triggerAlert() {
  alertActive = true;
  alertStartTime = millis();

  Serial.print("FALL CONFIRMED! Peak: ");
  Serial.print(peakG);
  Serial.println(" G");

  digitalWrite(BUZZER_PIN, HIGH);
}

// ===== Emergency Mode =====
void handleEmergencyState() {

  if (millis() - alertStartTime > ALERT_DURATION) {
    Serial.println("<======= Alert auto-stopped (EMERGENCY STATE)");
    alertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  digitalWrite(BUZZER_PIN, (millis() % 300 < 150) ? HIGH : LOW);
}

//PROTOTYPE V1.9