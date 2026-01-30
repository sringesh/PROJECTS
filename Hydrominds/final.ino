/*
  ESP32 Water Flow Meter with Firebase (Secure Auth)
  --------------------------------------------------
  - Measures flow rate + total liters
  - Saves to Preferences
  - Sends securely to Firebase Realtime Database
*/

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Preferences.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ------------------ USER CONFIG ------------------
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Firebase config
#define API_KEY "YOUR_FIREBASE_WEB_API_KEY"
#define DATABASE_URL "https://YOUR_PROJECT_ID.firebaseio.com"
#define USER_EMAIL "esp32@myproject.com"
#define USER_PASSWORD "12345678"

// --------------------------------------------------
const int PIN_FLOW = 27;
const unsigned long SAMPLE_MS = 1000;
const unsigned long SAVE_INTERVAL_MS = 10000;
float PULSES_PER_LITER = 450.0;

volatile unsigned long pulseCount = 0;
volatile unsigned long totalPulses = 0;
unsigned long lastMillis = 0;
unsigned long saveMillis = 0;

Preferences prefs;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ------------------ INTERRUPT ------------------
void IRAM_ATTR pulseISR() {
  pulseCount++;
  totalPulses++;
}

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_FLOW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), pulseISR, RISING);

  // Load previous total
  prefs.begin("water", false);
  unsigned long saved = prefs.getULong("totalP", 0);
  noInterrupts();
  totalPulses = saved;
  interrupts();
  Serial.print("Restored total pulses: ");
  Serial.println(saved);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  lastMillis = millis();
  saveMillis = millis();
}

// ------------------ LOOP ------------------
void loop() {
  unsigned long now = millis();

  if (now - lastMillis >= SAMPLE_MS) {
    detachInterrupt(digitalPinToInterrupt(PIN_FLOW));
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW), pulseISR, RISING);

    float flowLpm = ((float)pulses / PULSES_PER_LITER) * (60000.0 / (float)SAMPLE_MS);
    noInterrupts();
    unsigned long tp = totalPulses;
    interrupts();
    float totalLiters = (float)tp / PULSES_PER_LITER;

    Serial.print("Flow: ");
    Serial.print(flowLpm, 2);
    Serial.print(" L/min\tTotal: ");
    Serial.print(totalLiters, 3);
    Serial.println(" L");

    if (now - saveMillis >= SAVE_INTERVAL_MS) {
      prefs.putULong("totalP", tp);
      sendToFirebase(totalLiters);
      saveMillis = now;
    }

    lastMillis = now;
  }

  delay(10);
}

// ------------------ FIREBASE UPLOAD ------------------
void sendToFirebase(float totalLiters) {
  if (Firebase.ready()) {
    String path = "/water_meter/total_liters";
    if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), totalLiters)) {
      Serial.println(" Sent to Firebase: " + String(totalLiters, 3) + " L");
    } else {
      Serial.println(" Firebase error: " + fbdo.errorReason());
    }
  } else {
    Serial.println("Firebase not ready!");
  }
}

// ------------------ RESET (Optional) ------------------
void resetTotal() {
  noInterrupts();
  totalPulses = 0;
  interrupts();
  prefs.putULong("totalP", 0);
  Serial.println("Total reset to 0");
}