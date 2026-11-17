#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Wi-Fi & Public MQTT Broker Configuration
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "apu/fyp/jay/shm_wing_01/telemetry";

// Display & Sensor Objects
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

WiFiClient espClient;
PubSubClient mqtt(espClient);

const int buttonPin = 33;
const int motorPin = 25;

bool isVibrating = false;
bool lastButtonState = HIGH;
unsigned long lastTelemetryMs = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }
}

void connectMQTT() {
  while (!mqtt.connected()) {
    String clientId = "ESP32_SHM_" + String(random(0xffff), HEX);
    if (!mqtt.connect(clientId.c_str())) {
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  if (!mpu.begin()) {
    for (;;);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  updateOLED("CONNECTING...", 0.00);

  connectWiFi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  connectMQTT();
  updateOLED("MQTT ONLINE", 0.00);
}

void loop() {
  connectWiFi();
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  bool currentButtonState = digitalRead(buttonPin);
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float z_accel = a.acceleration.z;

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    isVibrating = !isVibrating;
    delay(200);
  }

  if (isVibrating) {
    analogWrite(motorPin, 200);
    updateOLED("VIBRATING...", z_accel);
  } else {
    analogWrite(motorPin, 0);
    updateOLED("MQTT ONLINE", z_accel);
  }

  // Publish JSON telemetry every 250 ms
  if (millis() - lastTelemetryMs >= 250) {
    lastTelemetryMs = millis();
    String payload = "{\"status\":\"" + String(isVibrating ? "VIBRATING" : "READY") +
                     "\",\"z_accel\":" + String(z_accel, 2) + "}";
    mqtt.publish(MQTT_TOPIC, payload.c_str());
  }

  lastButtonState = currentButtonState;
  delay(30);
}

void updateOLED(String statusText, float z_accel) {
  display.clearDisplay();
  display.setCursor(0, 5);
  display.print("TinyML SHM MQTT");
  display.drawLine(0, 15, 128, 15, SSD1306_WHITE);

  display.setCursor(0, 25);
  display.print("State: ");
  display.print(statusText);

  display.setCursor(0, 45);
  display.print("Z-Accel: ");
  display.print(z_accel, 2);
  display.print(" m/s^2");
  display.display();
}