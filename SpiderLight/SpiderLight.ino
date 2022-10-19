#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
// TODO add other headers - or error
#endif
#include <WiFiClient.h>
#include <OneBitDisplay.h>  // https://github.com/bitbank2/OneBitDisplay/wiki/API-and-constants
#include <Time.h>
#include "NtpTime.h"
#include "WifiConfig.h"
#include "LightingConfig.h"

#include "index.html.h"
#include "spider-light.css.h"
#include "spider-light-lib.js.h"


#define LED_B1 D5
#define LED_B2 D6
#define LED_A1 D7
#define LED_A2 D8



ESP8266WebServer server(80);
OBDISP obd;
uint8_t obdBackBuffer[1024];
LightingConfig lightingConfig;

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}
void handleCss() {
  server.send(200, "text/css", SPIDER_LIGHT_CSS);
}
void handleJsLib() {
  server.send(200, "text/javascript", SPIDER_LIGHT_LIB_JS);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "!GET";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleGetConfig() {
  String buf;
  serializeJson(lightingConfig.toJson(), buf);
  server.send(200, F("application/json"), buf);
}

void handlePutConfig() {
  // char buf[2048];
  // size_t len = client->read((uint8_t*)buf, sizeof(buf));
  // if (len > 0) {
  //   Serial.printf("(<%d> chars)", (int)len);
  //   Serial.write(buf, len);
  // }
  
  // server.arg();
  // lightingConfig.fromJson(...);
  // configNptTime(lightingConfig.timezone.c_str(), NULL);
  String payload = server.arg("plain");

  Serial.print("GOT CONFIG UPDATE ");
  char time_output[30];
  strftime(time_output, 30, "%a %Y-%m-%d %T", getNtpTime());
  Serial.println(time_output);
  Serial.println(payload);
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(200, F("application/json"), F("{\"ok\": false, \"error\":\"Invalid JSON\"}"));
    return;
  }
  LightingConfig newConfig;
  FromJsonResult result = newConfig.fromJson(doc);
  if (result.ok) {
    lightingConfig = newConfig;
    doc = lightingConfig.toJson();
    doc["ok"] = true;
    String buf;
    serializeJson(doc, buf);
    server.send(200, F("application/json"), buf);
    Serial.println("> config update OK");
  } else {
    char sz[128];
    sprintf(sz, "{\"ok\": false, \"error\":\"%s\"}", result.error.c_str());
    server.send(200, F("application/json"), sz);
    Serial.print("> config update ERROR - ");
    Serial.println(sz);
    // Serial.println(result.error.c_str());
  }
}

uint32_t ticks(0);

void updateDisplayTime(const tm* now) {
  char sz[12];
  strftime(sz, 12, (ticks % 2 == 0 ? "%H:%M" : "%H %M"), now);
  obdWriteString(&obd, 0, 128 - 5 * 8, 0, sz, FONT_8x8, 0, 0);
  strftime(sz, 12, "%A", now);
  obdWriteString(&obd, 0, 128 - strlen(sz) * 6, 8, sz, FONT_6x8, 0, 0);
}

// https://www.metageek.com/training/resources/understanding-rssi/
void updateDispalayWifiStatus() {
  obdWriteString(&obd, 0, 0, 0, "WiFi", FONT_8x8, 0, 0);
  if (WiFi.status() != WL_CONNECTED) {
    if (ticks % 2 == 0) {
      obdWriteString(&obd, 0, 0, 8, " NC", FONT_6x8, 0, 0);
    }
  } else {
    char sz[6];
    int32_t rssi = WiFi.RSSI();
    if (rssi > -80 || ticks % 2 == 0) {  // bink if the signal is "poor"
      sprintf(sz, "%ddb", rssi);
      obdWriteString(&obd, 0, 0, 8, sz, FONT_6x8, 0, 0);
    }
  }
}

void updateColorMix(const tm* now) {
  ChannelOutput vals(lightingConfig.calcOutputs(now));
  const int a1(vals.pinA1 * 1023);
  const int a2(vals.pinA2 * 1023);
  const int b1(vals.pinB1 * 1023);
  const int b2(vals.pinB2 * 1023);
  char sz[16];
  sprintf(sz, "w: %4d %4d", a1, b1);
  // sprintf(sz, "w: %.3f %.3f", vals.pinA1, vals.pinB1);
  obdWriteString(&obd, 0, 0, 16, sz, FONT_8x8, 0, 0);
  sprintf(sz, "c: %4d %4d", a2, b2);
  // sprintf(sz, "c: %.3f %.3f", vals.pinA2, vals.pinB2);
  obdWriteString(&obd, 0, 0, 24, sz, FONT_8x8, 0, 0);
  sprintf(sz, "time: %02d:%02d", vals.currentMin / 60, vals.currentMin % 60);
  obdWriteString(&obd, 0, 0, 32, sz, FONT_8x8, 0, 0);
  analogWrite(LED_A1, a1);
  analogWrite(LED_A2, a2);
  analogWrite(LED_B1, b1);
  analogWrite(LED_B2, b2);
}  

// called about twice per second from loop()
void tick() {
  memset(obdBackBuffer, 0x00, sizeof(obdBackBuffer));
  ticks++;
  updateDispalayWifiStatus();
  const tm* now = getNtpTime();
  updateDisplayTime(now);
  updateColorMix(now);
  obdDumpBuffer(&obd, NULL);
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);  // This LED is on/off inverted
  pinMode(LED_A1, OUTPUT);
  pinMode(LED_A2, OUTPUT);
  pinMode(LED_B1, OUTPUT);
  pinMode(LED_B2, OUTPUT);
  analogWriteFreq(2048);
  analogWriteResolution(10 /* bits */);
  analogWrite(LED_A1, 0);  // [0..1024)
  analogWrite(LED_A2, 0);  // [0..1024)
  analogWrite(LED_B1, 0);  // [0..1024)
  analogWrite(LED_B2, 0);  // [0..1024)
  
  lightingConfig.addScheduleEntry(450, 2700, 0);
  lightingConfig.addScheduleEntry(480, 3500, 80);
  lightingConfig.addScheduleEntry(720, 5800, 100);
  lightingConfig.addScheduleEntry(780, 5800, 100);
  lightingConfig.addScheduleEntry(1020, 4800, 90);
  lightingConfig.addScheduleEntry(1140, 4800, 70);
  lightingConfig.addScheduleEntry(1170, 2700, 0);
  lightingConfig.channelConfigB.maxBrightness = 35;

  
  Serial.begin(115200);
  // while (!Serial) delay(20);
  delay(500);
  
  Serial.println();
  Serial.println("LIGHTING CONFIG");
  serializeJson(lightingConfig.toJson(), Serial);
  Serial.println();  

  // init display
  obdI2CInit(&obd, OLED_128x64, -1, 0, 0, 1, -1, -1, -1, 400000L);  // probably can go to 800kHz
  // clear the display buffer and dispaly
  memset(obdBackBuffer, 0x00, sizeof(obdBackBuffer));
  obdSetBackBuffer(&obd, obdBackBuffer);
  obdDumpBuffer(&obd, NULL);

  // init WiFi
  WiFi.mode(WIFI_STA);
  // Downgraded from N to G for compatabiliy with ASUS routers
  // https://github.com/esp8266/Arduino/issues/8299
  // https://github.com/esp8266/Arduino/issues/8412
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.begin(STASSID, STAPSK);
  Serial.println("");
  Serial.println("Waiting for WiFi");
  obdWriteString(&obd, 0, 0, 0, "Waiting for WiFi", FONT_6x8, 0, 1);

  // Wait for WiFi connection
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, dotCount % 2);
    Serial.print('.');
    if (++dotCount > 120) {
      Serial.printf(" STATUS: %d\n", WiFi.status());
      dotCount = 0;
    }
  }
  digitalWrite(LED_BUILTIN, 1);  // OFF
  Serial.println("");
  Serial.printf("Connected to %s (%d db)\n", STASSID, WiFi.RSSI());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  char szTemp[32];
  sprintf(szTemp, "IP: %s", WiFi.localIP().toString().c_str());
  //  obdWriteString(&obd, 0, 0, 8, szTemp, FONT_6x8, 0, 1);

  // Wait for accurate time info
  Serial.println("Waiting for NTP time update");
  obdWriteString(&obd, 0, 0, 0, " Syncing Clock  ", FONT_6x8, 0, 1);
  configNptTime(lightingConfig.timezone.c_str(), NULL);
  dotCount = 0;
  while (!syncNtpTime()) {
    delay(500);
    digitalWrite(LED_BUILTIN, dotCount % 2);
    Serial.print('.');
    if (++dotCount > 20) {
      Serial.println("");
      Serial.print("Timed out waiting for time!");
      break;
    }
  }
  digitalWrite(LED_BUILTIN, 1);  // OFF
  Serial.println("");
  Serial.print("Current Time: ");
  char time_output[30];
  strftime(time_output, 30, "%a %Y-%m-%d %T", getNtpTime());
  Serial.println(time_output);

  if (MDNS.begin("spood")) {
    Serial.println("MDNS responder started - http://spood.local");
  }

  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  server.on("/spider-light.css", handleCss);
  server.on("/spider-light-lib.js", handleJsLib);
  server.onNotFound(handleNotFound);
  server.on("/config.json", HTTPMethod::HTTP_GET, handleGetConfig);
  server.on("/config.json", HTTPMethod::HTTP_PUT, handlePutConfig);

  server.begin();
  Serial.println("HTTP server started");
}

uint32_t lastTick(0);
void loop(void) {
  server.handleClient();
  MDNS.update();
  if (millis() - lastTick >= 500ul) {
    lastTick = millis();
    tick();
  }
}
