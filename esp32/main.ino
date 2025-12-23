#include <WiFi.h>
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <HTTPClient.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SPI.h>

#define WIFI_SSID "A1105-2"
#define WIFI_PASSWORD "87654321"

#define MQTT_HOST IPAddress(192,168,1,11)
#define MQTT_PORT 1883

bool connectMqtt = false;
String lastLink = "";

String pendingLink = "";
volatile bool newSongReady = false;

TFT_eSPI tft = TFT_eSPI();

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case 16:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectMqtt = true;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        break;
    }
}

bool jpegDrawCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap){
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}


void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe("spotify/nowplaying", 2);
  // Serial.print("Subscribing at QoS 2, packetId: ");
  // Serial.println(packetIdSub);
}

const char* getValueFromJson(const char* payload, const char* key) {
  static StaticJsonDocument<1024> doc;

  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON error: ");
    Serial.println(err.c_str());
    return nullptr;
  }

  return doc[key];
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void coverTask(void *param) {
  for (;;) {
    if (newSongReady) {
      newSongReady = false;

      Serial.println("🎵 New Song!");
      // Serial.println(pendingLink);

      if (downloadCover(pendingLink)) {
        drawCover();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

bool downloadCover(String url){
    Serial.println("Downloading cover !");
    HTTPClient http;
    // http.setTimeout(8000);
    http.begin("https://compress.hansprojects.com/album?url=" + url);

    int code = http.GET();
    if (code != 200)
    {
        Serial.printf("HTTP failed: %d\n", code);
        http.end();
        return false;
    }

    Serial.println("GET method done !");

    WiFiClient *stream = http.getStreamPtr();
    File f = LittleFS.open("/cover", FILE_WRITE);
    if (!f)
    {
        Serial.println("FS write failed");
        http.end();
        return false;
    }

    Serial.println("FS Done !");

    uint8_t buf[1024];
    size_t total = 0;

    while (true) {
        int len = stream->readBytes(buf, sizeof(buf));
        if (len <= 0) break;

        f.write(buf, len);
        total += len;
        Serial.println(len);
    }

    f.close();
    http.end();

    Serial.printf("Downloaded %u bytes\n", total);
    return total > 0;
}

bool drawCover()
{
  Serial.println("Begin Drawing !");
  File f = LittleFS.open("/cover", "r");
  if (!f)
  {
      Serial.println("Failed to open JPG");
      return false;
  }
  Serial.println("Done init LittleFS");
  size_t size = f.size();
  uint8_t *buf = (uint8_t *)malloc(size);
  if (!buf)
  {
      Serial.println("malloc failed");
      f.close();
      return false;
  }

  Serial.println("Done Malloc !");

  f.read(buf, size);
  f.close();

  int result = TJpgDec.drawJpg(0, 0, buf, size);
  free(buf);

  return result == 0;
}

void onMqttMessage(
  char* topic,
  char* payload,
  AsyncMqttClientMessageProperties properties,
  size_t len,
  size_t index,
  size_t total
) {
  if (index != 0) return;

  const char* linkChar = getValueFromJson(payload, "link");

  // BE publish "" → tidak ada lagu baru
  if (!linkChar || strlen(linkChar) == 0) return;

  pendingLink = String(linkChar);
  newSongReady = true;
}

void setup() {
  Serial.begin(115200);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  LittleFS.begin(true);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpegDrawCallback);

  connectToWifi();

  xTaskCreatePinnedToCore(
    coverTask,
    "CoverTask",
    8192,
    NULL,
    1,
    NULL,
    1 
  );

}

void loop() {

  if(WiFi.isConnected() && connectMqtt){
    connectMqtt = false;
    connectToMqtt();
  }

  delay(500);
}
