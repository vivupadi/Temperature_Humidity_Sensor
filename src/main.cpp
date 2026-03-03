#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ─── Pin Configuration ────────────────────────────────────────
#define DHTPIN        4
#define DHTTYPE       DHT11
#define LED_PIN       2

// ─── MQTT Configuration ───────────────────────────────────────
#define MQTT_PORT     8883
#define MQTT_TOPIC    "sensors/dht"
#define DEVICE_ID     "esp32-home"

// ─── Thresholds ───────────────────────────────────────────────
#define TEMP_THRESHOLD      20.0
#define HUMIDITY_THRESHOLD  65.0

// ─── Timing ───────────────────────────────────────────────────
#define READ_INTERVAL_MS    300000

// ─── Objects ──────────────────────────────────────────────────
DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure espClient;       // TLS client for HiveMQ port 8883
PubSubClient mqtt(espClient);

unsigned long lastReadTime = 0;
int readingCount = 0;

// ─── WiFi Connect ─────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("\nConnecting to WiFi: %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(" Failed! Check WiFi credentials.");
  }
}

// ─── MQTT Connect ─────────────────────────────────────────────
void connectMQTT() {
  espClient.setInsecure();  // Skip SSL cert validation (fine for dev)
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);

  Serial.printf("Connecting to MQTT: %s ", MQTT_HOST);

  int attempts = 0;
  while (!mqtt.connected() && attempts < 5) {
    if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(" Connected!");
    } else {
      Serial.printf(" Failed (rc=%d), retrying...\n", mqtt.state());
      delay(3000);
      attempts++;
    }
  }
}

// ─── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();

  Serial.println("=====================================");
  Serial.println("  DHT11 Monitor - Phase 2 MQTT");
  Serial.println("=====================================");

  // Startup blink - 3 flashes = device ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(300);
    digitalWrite(LED_PIN, LOW);  delay(300);
  }

  connectWiFi();
  connectMQTT();

  Serial.println("Warming up DHT11 sensor...");
  delay(2000);  // DHT11 needs 2 seconds after power-on
  Serial.println("\nSystem ready. Publishing to MQTT...\n");
}

// ─── Main Loop ────────────────────────────────────────────────
void loop() {
  // Reconnect if dropped
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    connectWiFi();
  }
  if (!mqtt.connected()) {
    Serial.println("MQTT lost. Reconnecting...");
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = now;
    readingCount++;

    // LED ON -> reading in progress
    digitalWrite(LED_PIN, HIGH); delay(500);

    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();

    // Debug output
    Serial.printf("Raw temp: %f\n", temperature);
    Serial.printf("Raw humidity: %f\n", humidity);
    Serial.printf("isnan temp: %d\n", isnan(temperature));
    Serial.printf("isnan humidity: %d\n", isnan(humidity));

    // LED OFF -> done
    digitalWrite(LED_PIN, LOW);

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("ERROR: Sensor read failed! Check wiring.");
      return;
    }

    // ── Build JSON payload ────────────────────────────────
    bool tempAlert = temperature > TEMP_THRESHOLD;
    bool humAlert  = humidity    > HUMIDITY_THRESHOLD;

    char payload[200];
    snprintf(payload, sizeof(payload),
      "{\"device\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,"
      "\"temp_alert\":%s,\"humidity_alert\":%s}",
      DEVICE_ID,
      temperature,
      humidity,
      tempAlert ? "true" : "false",
      humAlert  ? "true" : "false"
    );

    // ── Publish to MQTT ───────────────────────────────────
    bool published = mqtt.publish(MQTT_TOPIC, payload);

    // ── Serial log ────────────────────────────────────────
    Serial.printf("-- Reading #%d --\n", readingCount);
    Serial.printf("  Temperature : %.1f C %s\n", temperature, tempAlert ? "ALERT" : "OK");
    Serial.printf("  Humidity    : %.1f %%  %s\n", humidity,  humAlert  ? "ALERT" : "OK");
    Serial.printf("  Payload     : %s\n", payload);
    Serial.printf("  Published   : %s\n\n", published ? "Success" : "Failed");
  }
}