// ESP32-CAM Face Detection + WhatsApp Alert (merged example)
// AI Thinker ESP32-CAM

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASSCODE";

const char* API_KEY = "CIRCUIT DIGEST API KEY";
const char* serverName = "www.circuitdigest.cloud";

const char* FACE_PATH = "/api/v1/face-detection/detect";
const char* WA_PATH   = "/api/v1/whatsapp/send";

#define TRIGGER_BTN 13

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

WiFiClientSecure client;
unsigned long lastTrigger = 0;

void initCamera() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = Y2_GPIO_NUM;
  cfg.pin_d1 = Y3_GPIO_NUM;
  cfg.pin_d2 = Y4_GPIO_NUM;
  cfg.pin_d3 = Y5_GPIO_NUM;
  cfg.pin_d4 = Y6_GPIO_NUM;
  cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM;
  cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk = XCLK_GPIO_NUM;
  cfg.pin_pclk = PCLK_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM;
  cfg.pin_href = HREF_GPIO_NUM;
  cfg.pin_sscb_sda = SIOD_GPIO_NUM;
  cfg.pin_sscb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn = PWDN_GPIO_NUM;
  cfg.pin_reset = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = FRAMESIZE_VGA;
  cfg.jpeg_quality = 10;
  cfg.fb_count = 1;

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("Camera init failed");
    while (1);
  }
}

void sendWhatsApp(int faceCount) {
  WiFiClientSecure wa;
  wa.setInsecure();

  if (!wa.connect(serverName, 443)) {
    Serial.println("WhatsApp connection failed");
    return;
  }

  String payload =
    "{\"phone_number\":\"YOUR_NUMBER\","
    "\"template_id\":\"threshold_violation_alert\","
    "\"variables\":{"
    "\"device_name\":\"ESP32-CAM\","
    "\"parameter\":\"Faces Detected\","
    "\"measured_value\":\"" + String(faceCount) + "\","
    "\"limit\":\"0\","
    "\"location\":\"Camera\"}}";

  wa.println("POST " + String(WA_PATH) + " HTTP/1.1");
  wa.println("Host: " + String(serverName));
  wa.println("X-API-Key: " + String(API_KEY));
  wa.println("Content-Type: application/json");
  wa.println("Connection: close");
  wa.print("Content-Length: ");
  wa.println(payload.length());
  wa.println();
  wa.print(payload);

  Serial.println("WhatsApp alert sent.");
  wa.stop();
}

void detectFaces() {
  for (int i = 0; i < 3; i++) {
    camera_fb_t *tmp = esp_camera_fb_get();
    if (tmp) esp_camera_fb_return(tmp);
    delay(200);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    return;
  }

  if (!client.connect(serverName, 443)) {
    Serial.println("API connection failed");
    esp_camera_fb_return(fb);
    return;
  }

  String boundary = "----ESP32Boundary";
  String head =
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"imageFile\"; filename=\"snap.jpg\"\r\n"
      "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  int contentLen = head.length() + fb->len + tail.length();

  client.println("POST " + String(FACE_PATH) + " HTTP/1.1");
  client.println("Host: " + String(serverName));
  client.println("X-API-Key: " + String(API_KEY));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLen));
  client.println("Connection: close");
  client.println();

  client.print(head);
  client.write(fb->buf, fb->len);
  client.print(tail);

  esp_camera_fb_return(fb);

  unsigned long start = millis();
  while (!client.available()) {
    if (millis() - start > 15000) {
      client.stop();
      return;
    }
  }

  String response;
  while (client.available()) response += (char)client.read();
  client.stop();

  int idx = response.indexOf("\"face_count\":");
  int faceCount = 0;

  if (idx >= 0) {
    faceCount = response.substring(idx + 13).toInt();
  }

  Serial.print("Faces detected: ");
  Serial.println(faceCount);

  if (faceCount > 0) {
    sendWhatsApp(faceCount);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIGGER_BTN, INPUT_PULLUP);

  initCamera();

  client.setInsecure();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
}

void loop() {
  if (digitalRead(TRIGGER_BTN) == LOW &&
      millis() - lastTrigger > 1000) {

    lastTrigger = millis();

    Serial.println("Capturing...");
    detectFaces();
  }
}
