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
#include <Wire.h>
#include "I2C_eeprom.h"
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


I2C_eeprom eeprom(0x50, I2C_DEVICESIZE_24LC04 /* 512 bytes */);
ESP8266WebServer server(80);
OBDISP obd;
uint8_t obdBackBuffer[1024];
LightingConfig lightingConfig;

bool saveLightingConfigToEeprom();

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
  StatusOrError result = newConfig.fromJson(doc);
  if (result.ok) {
    lightingConfig = newConfig;
    doc = lightingConfig.toJson();
    doc["ok"] = true;
    if (eeprom.isConnected()) {
      if (!saveLightingConfigToEeprom()) {
        doc["error"] = "WARN: Failed to write config to EEPROM, config changes will not persist through resets.";
      } else {
        doc["error"] = "OK: Written to EEPROM.";
      }
    } else {
      doc["error"] = "WARN: EEPROM not connected, config changes will not persist through resets.";
    }
    String buf;
    serializeJson(doc, buf);
    server.send(200, F("application/json"), buf);
    Serial.print("> config update ");
    Serial.println(doc["error"].as<char*>());
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

void defaultLightingConfig() {
  Serial.println(F("Loading default lighting config"));
  lightingConfig = LightingConfig();
  lightingConfig.addScheduleEntry(450, 2700, 0);
  lightingConfig.addScheduleEntry(480, 3500, 80);
  lightingConfig.addScheduleEntry(720, 5800, 100);
  lightingConfig.addScheduleEntry(780, 5800, 100);
  lightingConfig.addScheduleEntry(1020, 4800, 90);
  lightingConfig.addScheduleEntry(1140, 4800, 70);
  lightingConfig.addScheduleEntry(1170, 2700, 0);
  lightingConfig.channelConfigA.maxBrightness = 67;
  lightingConfig.channelConfigB.maxBrightness = 33;
  serializeJson(lightingConfig.toJson(), Serial);
}

void loadLightingConfigFromEeprom() {
  Serial.println(F("Loading lighting config from EEPROM"));
  LightingConfigEepromHeader header;
  eeprom.readBlock(0, (uint8_t *) &header, sizeof(header));
  StatusOrError soe = LightingConfig::validateHeader(header);
  if (!soe.ok) {
    Serial.print(F("Failed to load config from EEPROM - "));
    Serial.println(soe.error.c_str());
    defaultLightingConfig();
    return;
  }
  byte data[header.size];
  eeprom.readBlock(sizeof(header), data, header.size);
  StatusOrError soe2 = lightingConfig.deserialize(header, data);
  if (soe2.ok) {
    Serial.printf("Deserialized raw size %d bytes %s\n", header.size, soe2.error.c_str());
    serializeJson(lightingConfig.toJson(), Serial);
  } else {
    Serial.printf("Deserialize FAILED - %s\n", soe2.error.c_str());
  }
}

bool saveLightingConfigToEeprom() {
  const auto header = lightingConfig.calcEepromDataHeader();
  byte buff[header.size];
  uint16_t crc(0);
  int wroteBytes = lightingConfig.serialize(buff, crc);
  if (wroteBytes == header.size) {
    Serial.printf("Serialize OK - seralized config into %d bytes with crc 0x%04X\n", header.size, crc);
    if (eeprom.writeBlock(0, (uint8_t *) &header, sizeof(header))) {
      return false;
    }
    if (eeprom.writeBlock(sizeof(header), buff, header.size)) {
      return false;
    }

    // Serial.println("Serialize OK - echoing deserialized version");
    // LightingConfig lc2;
    // lc2.deserialize(header, buff);
    // serializeJson(lc2.toJson(), Serial);
  } else {
    Serial.printf("Serialize ERROR expected %d bytes, wrote %d bytes", header.size, wroteBytes);
    return false;
  }
  return true;
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
  
  Serial.begin(115200);
  // while (!Serial) delay(20);
  delay(500);
  Serial.print("\n\n");
  
  eeprom.begin();
  if (eeprom.isConnected()) {
    Serial.println(F("EEPROM connected"));
    loadLightingConfigFromEeprom();
  } else {
    Serial.println(F("WARN: Can't find EEPROM"));
    defaultLightingConfig();
  }


  // const auto header = lightingConfig.calcEepromDataHeader();
  // Serial.printf("serialized config size: %d\n", header.size);
  // byte buff[header.size];
  // int wroteBytes = lightingConfig.serialize(buff);
  // if (wroteBytes == header.size) {
  //   Serial.println("Serialize OK - echoing deserialized version");
  //   LightingConfig lc2;
  //   lc2.deserialize(header, buff);
  //   serializeJson(lc2.toJson(), Serial);
  // } else {
  //   Serial.printf("Serialize ERROR expected %d bytes, wrote %d bytes", header.size, wroteBytes);
  // }

  // init display
  obdI2CInit(&obd, OLED_128x64, -1, 0, 0, 1, -1, -1, -1, 400000L);
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
