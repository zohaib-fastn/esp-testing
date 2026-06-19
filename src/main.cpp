#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

const char* ssid = "MERCUSYS_5E58";
const char* password = "94306750";
const char* webhookUrl = "http://webhook.site/97732e3d-c284-4fe3-ad06-f3024ccbca3c";
const char* otaHostname = "esp32";
const char* mdnsHostname = "esp32";

#define LED_PIN 2

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
bool doorLocked = true;

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Door Lock</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #0a192f, #112240, #1a365d);
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
    }
    .card {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 24px;
      padding: 48px 40px;
      text-align: center;
      box-shadow: 0 8px 32px rgba(0,0,0,0.4);
      min-width: 340px;
      max-width: 400px;
    }
    .lock-icon {
      font-size: 80px;
      margin-bottom: 16px;
      display: block;
    }
    h1 { font-size: 26px; margin-bottom: 4px; font-weight: 600; }
    .subtitle { color: #999; font-size: 14px; margin-bottom: 32px; }
    .status-bar {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      padding: 14px 24px;
      border-radius: 12px;
      margin-bottom: 28px;
      font-size: 18px;
      font-weight: 600;
    }
    .status-locked {
      background: rgba(72, 219, 251, 0.15);
      border: 1px solid rgba(72, 219, 251, 0.3);
      color: #48dbfb;
    }
    .status-unlocked {
      background: rgba(255, 159, 67, 0.15);
      border: 1px solid rgba(255, 159, 67, 0.3);
      color: #ff9f43;
    }
    .dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      display: inline-block;
    }
    .dot.locked { background: #48dbfb; box-shadow: 0 0 8px #48dbfb; }
    .dot.unlocked { background: #ff9f43; box-shadow: 0 0 8px #ff9f43; }
    .btn {
      display: block;
      width: 100%;
      padding: 16px;
      font-size: 18px;
      border: none;
      border-radius: 14px;
      cursor: pointer;
      color: #fff;
      font-weight: 700;
      text-decoration: none;
      text-align: center;
      margin: 10px 0;
      transition: transform 0.1s, box-shadow 0.2s;
      letter-spacing: 0.5px;
    }
    .btn:active { transform: scale(0.97); }
    .btn-unlock {
      background: linear-gradient(135deg, #ff9f43, #f7b731);
      box-shadow: 0 4px 15px rgba(247, 183, 49, 0.3);
    }
    .btn-lock {
      background: linear-gradient(135deg, #48dbfb, #0abde3);
      box-shadow: 0 4px 15px rgba(10, 189, 227, 0.3);
    }
    .btn-ota {
      background: linear-gradient(135deg, #ff6348, #ff4757);
      box-shadow: 0 4px 15px rgba(255, 71, 87, 0.3);
      font-size: 14px;
      padding: 12px;
    }
    .divider {
      border: none;
      border-top: 1px solid rgba(255,255,255,0.08);
      margin: 24px 0 16px;
    }
    .info {
      font-size: 12px;
      color: #555;
      line-height: 1.8;
    }
    .badges {
      margin-top: 12px;
      display: flex;
      gap: 8px;
      justify-content: center;
      flex-wrap: wrap;
    }
    .badge {
      display: inline-block;
      padding: 4px 12px;
      border-radius: 20px;
      font-size: 11px;
    }
    .badge-webhook {
      background: rgba(46, 213, 115, 0.2);
      border: 1px solid rgba(46, 213, 115, 0.3);
      color: #2ed573;
    }
    .badge-ota {
      background: rgba(255, 99, 72, 0.2);
      border: 1px solid rgba(255, 99, 72, 0.3);
      color: #ff6348;
    }
  </style>
</head>
<body>
  <div class="card">
    <span class="lock-icon">%ICON%</span>
    <h1>Smart Door Lock</h1>
    <p class="subtitle">NodeMCU v3 IoT Controller</p>
    <div class="status-bar %STATUS_CLASS%">
      <span class="dot %DOT_CLASS%"></span>
      Door is %STATE%
    </div>
    <a class="btn btn-unlock" href="/unlock">OPEN DOOR</a>
    <a class="btn btn-lock" href="/lock">LOCK DOOR</a>
    <hr class="divider">
    <a class="btn btn-ota" href="/update">FIRMWARE UPDATE (OTA)</a>
    <hr class="divider">
    <div class="info">
      <p>IP: %IP% | Uptime: %UPTIME%s</p>
      <p>Free Memory: %HEAP% bytes</p>
      <p>Firmware: v%VERSION%</p>
    </div>
    <div class="badges">
      <span class="badge badge-webhook">Webhook Active</span>
      <span class="badge badge-ota">OTA Enabled</span>
    </div>
  </div>
</body>
</html>
)rawliteral";

#define FIRMWARE_VERSION "1.3.0"

void sendWebhook(const char* action) {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, webhookUrl);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"action\":\"";
  payload += action;
  payload += "\",\"device\":\"NodeMCU-DoorLock\",\"ip\":\"";
  payload += WiFi.localIP().toString();
  payload += "\",\"firmware\":\"v";
  payload += FIRMWARE_VERSION;
  payload += "\",\"uptime\":";
  payload += String(millis() / 1000);
  payload += "}";

  int httpCode = http.POST(payload);
  Serial.printf("[Webhook] %s -> HTTP %d\n", action, httpCode);
  http.end();
}

String buildPage() {
  String page = FPSTR(webpage);
  page.replace("%STATE%", doorLocked ? "LOCKED" : "UNLOCKED");
  page.replace("%STATUS_CLASS%", doorLocked ? "status-locked" : "status-unlocked");
  page.replace("%DOT_CLASS%", doorLocked ? "locked" : "unlocked");
  page.replace("%ICON%", doorLocked ? "&#128274;" : "&#128275;");
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%UPTIME%", String(millis() / 1000));
  page.replace("%HEAP%", String(ESP.getFreeHeap()));
  page.replace("%VERSION%", FIRMWARE_VERSION);
  return page;
}

void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void handleUnlock() {
  doorLocked = false;
  digitalWrite(LED_PIN, LOW);
  sendWebhook("DOOR_OPENED");
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleLock() {
  doorLocked = true;
  digitalWrite(LED_PIN, HIGH);
  sendWebhook("DOOR_LOCKED");
  server.sendHeader("Location", "/");
  server.send(302);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("\n=============================");
  Serial.println("  Smart Door Lock Server");
  Serial.printf("  Firmware: v%s\n", FIRMWARE_VERSION);
  Serial.println("=============================");

  Serial.printf("Connecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("\nFailed to connect! Status: %d\n", WiFi.status());
    delay(5000);
    ESP.restart();
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(mdnsHostname)) {
    Serial.printf("mDNS started: http://%s.local\n", mdnsHostname);
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("mDNS failed to start!");
  }

  // --- ArduinoOTA (upload from PlatformIO/Arduino IDE wirelessly) ---
  ArduinoOTA.setHostname(otaHostname);
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete! Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // --- HTTP OTA (upload .bin from browser) ---
  httpUpdater.setup(&server, "/update");

  // Web routes
  server.on("/", handleRoot);
  server.on("/unlock", handleUnlock);
  server.on("/lock", handleLock);

  server.begin();
  Serial.println("Smart Door Lock server started!");
  Serial.println("Webhook notifications enabled.");
  Serial.println("OTA enabled (IDE + Browser).");
  Serial.printf("Browser OTA: http://%s/update\n", WiFi.localIP().toString().c_str());
}

void loop() {
  MDNS.update();
  ArduinoOTA.handle();
  server.handleClient();
}
