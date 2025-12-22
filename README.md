# ESP32 Spotify Album Display

![S__10002456](https://github.com/user-attachments/assets/178b5688-6496-4fc4-970e-47bd2f58376c)


A real-time **Spotify Now Playing album cover display** using **ESP32**, **MQTT**, and a **TFT screen**.

This project is designed to be **lightweight and reliable** by offloading all heavy image processing to a backend service.  
The ESP32 only handles:
- receiving song change events
- downloading a compressed album cover
- rendering it on the TFT display

---

## 🧠 System Architecture

### Components
- **ESP32**
  - Subscribes to `spotify/nowplaying`
  - Downloads JPEG album cover
  - Renders image using `TJpg_Decoder`
- **n8n**
  - Monitors Spotify “Currently Playing”
  - Publishes MQTT only when the song changes
- **Backend (Node.js + Sharp)**
  - Downloads Spotify image
  - Resizes & compresses it for ESP32

---

## 🖥️ Hardware Requirements

- ESP32
- TFT display (compatible with `TFT_eSPI`)
- WiFi connection

---

## 📦 Software Stack

### ESP32 Firmware
- Arduino Framework
- FreeRTOS (ESP32 native)
- AsyncMqttClient
- TFT_eSPI
- TJpg_Decoder
- ArduinoJson
- LittleFS

### Backend
- Node.js
- Express
- Axios
- Sharp

### Automation
- n8n
- MQTT Broker (Mosquitto / EMQX / HiveMQ)

---

### 📡 MQTT Topic
### Payload Format
```json
{
  "link": "https://i.scdn.co/image/xxxx"
}
```
If no new song is playing, the backend publishes:

```json
{
  "link": ""
}
```

🔌 ESP32 Firmware Overview
### Features
- Automatic WiFi and MQTT reconnection
- FreeRTOS task for non-blocking rendering
- JPEG streaming stored in LittleFS
- Stable and memory-efficient main loop
- Firmware Flow
- ESP32 connects to WiFi
- Connects to the MQTT broker
- Receives the album cover URL
- Downloads a compressed JPEG via HTTP
- Saves the image to LittleFS
- Renders the album cover on the TFT display

### Firmware Location
/esp32
 └── esp32_spotify_display.ino

 🌐 Image Proxy Backend (Node.js)
- The backend offloads heavy processing from the ESP32 by:
  - downloading Spotify album images
  - resizing images to 240×240
  - converting images to JPEG (ESP32-friendly format)
    
  HTTP Endpoint
  - GET /album?url=<spotify_image_url>

🔁 n8n Workflow

- The n8n workflow is responsible for:
  - polling Spotify Currently Playing
  - comparing the current track with the previous one
  - publishing an MQTT message only when the track changes

This approach prevents unnecessary MQTT traffic and saves ESP32 bandwidth.

### How To Run (backend server)

cd backend <br>
npm install <br>
node index.js
