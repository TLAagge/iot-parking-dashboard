#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <time.h>

#include "secrets.h"

const char* MQTT_TOPIC = "esp32/parking/test";

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// AWS IoT
WiFiClientSecure net;
PubSubClient mqttClient(net);

// SMHI
WiFiClientSecure smhiClient;

const char* SMHI_URL =
  "https://opendata-download-metobs.smhi.se/api/version/1.0/parameter/1/station/98230/period/latest-hour/data.json";

float currentTemperature = -999;
unsigned long lastWeatherFetch = 0;

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
}

void syncTime() {
  Serial.println("Synchronizing time...");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);

  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println();
  Serial.println("Time synchronized!");
}

void connectAWS() {
  net.setCACert(AWS_ROOT_CA);
  net.setCertificate(DEVICE_CERTIFICATE);
  net.setPrivateKey(DEVICE_PRIVATE_KEY);

  mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);

  Serial.println("Connecting to AWS IoT Core...");

  if (mqttClient.connect("ESP32-Parking")) {
    Serial.println("Connected to AWS IoT Core!");
  } else {
    Serial.print("AWS MQTT connection failed. State: ");
    Serial.println(mqttClient.state());
  }
}

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.0343 / 2;
}

float getSMHITemperature() {
  
  smhiClient.setInsecure();

  HTTPClient http;

  Serial.println("Getting temperature from SMHI...");

  if (!http.begin(smhiClient, SMHI_URL)) {
    Serial.println("Could not connect to SMHI.");
    return -999;
  }

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("SMHI HTTP error: ");
    Serial.println(httpCode);

    http.end();
    return -999;
  }

  String payload = http.getString();

  http.end();

  // Find the temperature value in the JSON response.

  int valuePos = payload.indexOf("\"value\":\"");

  if (valuePos == -1) {
    Serial.println("Could not find temperature in SMHI response.");
    return -999;
  }

  valuePos += 9;

  int endPos = payload.indexOf("\"", valuePos);

  if (endPos == -1) {
    Serial.println("Could not parse SMHI temperature.");
    return -999;
  }

  String temperatureString = payload.substring(valuePos, endPos);

  float temperature = temperatureString.toFloat();

  Serial.print("SMHI temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  return temperature;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 Parking - AWS IoT");
  Serial.println("==============================");

  connectWiFi();
  syncTime();
  connectAWS();

  // Get initial temperature from SMHI
  currentTemperature = getSMHITemperature();

  if (currentTemperature == -999) {
    Serial.println("SMHI temperature unavailable.");
  }
}

void loop() {
  mqttClient.loop();

  static unsigned long lastMessage = 0;

  // Update SMHI temperature once per minute.
  if (millis() - lastWeatherFetch >= 60000) {
    lastWeatherFetch = millis();

    float newTemperature = getSMHITemperature();

    if (newTemperature != -999) {
      currentTemperature = newTemperature;
    }
  }

  // Send parking measurement every 5 seconds.
  if (millis() - lastMessage >= 5000) {
    lastMessage = millis();

    float distance = readDistance();

    if (distance < 0) {
      Serial.println("No echo detected.");
      return;
    }

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    Serial.print("Temperature: ");

    if (currentTemperature == -999) {
      Serial.println("Unavailable");
    } else {
      Serial.print(currentTemperature);
      Serial.println(" °C");
    }

    char message[192];

    if (currentTemperature == -999) {
      snprintf(
        message,
        sizeof(message),
        "{\"device\":\"ESP32-Parking\",\"distance_cm\":%.2f}",
        distance
      );
    } else {
      snprintf(
        message,
        sizeof(message),
        "{\"device\":\"ESP32-Parking\",\"distance_cm\":%.2f,\"temperature_c\":%.2f}",
        distance,
        currentTemperature
      );
    }

    if (mqttClient.publish(MQTT_TOPIC, message)) {
      Serial.print("Published to AWS: ");
      Serial.println(message);
    } else {
      Serial.println("Publish failed!");
    }
  }
}
