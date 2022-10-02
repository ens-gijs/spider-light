#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
// TODO add other headers
#endif
#include <WiFiClient.h>
#include <OneBitDisplay.h>  // https://github.com/bitbank2/OneBitDisplay/wiki/API-and-constants
#include <Time.h>
#include "NtpTime.h"
#include "WifiConfig.h"


////
//// software PWM shit
////
//#if !defined(ESP8266)
//  #error This code is designed to run on ESP8266 and ESP8266-based boards! Please check your Tools->Board setting.
//#endif
//
//// These define's must be placed at the beginning before #include "ESP8266_PWM.h"
//// _PWM_LOGLEVEL_ from 0 to 4
//// Don't define _PWM_LOGLEVEL_ > 0. Only for special ISR debugging only. Can hang the system.
//#define _PWM_LOGLEVEL_                0
//
//#define USING_MICROS_RESOLUTION       true    //false
//
//// Default is true, uncomment to false
////#define CHANGING_PWM_END_OF_CYCLE     false
//
//// Select a Timer Clock
//#define USING_TIM_DIV1                true              // for shortest and most accurate timer
//#define USING_TIM_DIV16               false             // for medium time and medium accurate timer
//#define USING_TIM_DIV256              false             // for longest timer but least accurate. Default
//
//// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
//#include "ESP8266_PWM.h"
//
//#define HW_TIMER_INTERVAL_US      20L
//ESP8266Timer ITimer; // Init ESP8266Timer
//ESP8266_PWM ISR_PWM; // Init ESP8266_ISR_PWM
//void IRAM_ATTR TimerHandler() {ISR_PWM.run();}




// enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
// Hawaii Time HAW10
// Alaska Time AKST9AKDT
// Pacific Time  PST8PDT
// Mountain Time MST7MDT
// Mountain Time (Arizona, no DST) MST7
// Central Time  CST6CDT
// Eastern Time  EST5EDT
const char* TZ_INFO = "EST5EDT";

#define LED_A_WARM D5
#define LED_A_COOL D6
#define LED_B_WARM D7
#define LED_B_COOL D8

ESP8266WebServer server(80);
OBDISP obd;
uint8_t obdBackBuffer[1024];


void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!\r\n");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
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

#define ON_TIME_MIN (7 * 60)
#define OFF_TIME_MIN (20 * 60)
//#define ON_TIME_MIN (15)
//#define OFF_TIME_MIN (23 * 60 + 45)
#define NOON_TIME_START_MIN (12 * 60 - 30)
#define NOON_TIME_END_MIN (12 * 60 + 30)
// should be about 5600K (if the flux is equal)
#define NOON_COLOR_MIX 0.7632

#define BRIGHT_WAKE 0.01
// TODO: reduce this to 20-60 min after testing
#define BRIGHT_RAMP_MIN (45)
//#define BRIGHT_NOON (1.0 / NOON_COLOR_MIX)
#define BRIGHT_NOON 0.5

float interpellate(float a, float b, float p) {
  return a + p * (b - a);
}
float interpellateInv(int a, int b, int p) {
  int v = b - a;
  float w = p - a;
  return w / v;
}

void updateColorMix(const tm* now) {
  // TODO: shift DST out of the light schedule
//  int currentMin = (now->tm_hour * 60) + now->tm_min;
//  int currentMin = now->tm_sec * 24;  // simulate a day over one min
  bool oneMinSim(false);
  int currentMin;
  int cool(0);
  int warm(0);
  if (oneMinSim) {
    currentMin = ((now->tm_min % 5) * 60 + now->tm_sec)* 5;  // simulate a day over 5 min (ok, so it's a 25hr day simulated - meh)
  } else {
    currentMin = (now->tm_hour * 60) + now->tm_min;
  }
  if (currentMin >= ON_TIME_MIN && currentMin < OFF_TIME_MIN) {
    float colorMix;
    float brightness(BRIGHT_NOON);
    if (currentMin < NOON_TIME_START_MIN) {
      float p = interpellateInv(ON_TIME_MIN, NOON_TIME_START_MIN, currentMin);
      colorMix = interpellate(0, NOON_COLOR_MIX, p);
      if (currentMin < ON_TIME_MIN + BRIGHT_RAMP_MIN) {
        p = interpellateInv(ON_TIME_MIN, ON_TIME_MIN + BRIGHT_RAMP_MIN, currentMin);
        brightness = interpellate(BRIGHT_WAKE, BRIGHT_NOON, p);
      }
    } else if (currentMin >= NOON_TIME_START_MIN && currentMin < NOON_TIME_END_MIN) {
      colorMix = NOON_COLOR_MIX;
    } else {
      float p = interpellateInv(NOON_TIME_END_MIN, OFF_TIME_MIN, currentMin);
      colorMix = interpellate(NOON_COLOR_MIX, 0, p);
      if (currentMin >= OFF_TIME_MIN - BRIGHT_RAMP_MIN) {
        p = interpellateInv(OFF_TIME_MIN - BRIGHT_RAMP_MIN, OFF_TIME_MIN, currentMin);
        brightness = interpellate(BRIGHT_NOON, BRIGHT_WAKE, p);
      }
    }
    cool = (int) (colorMix * brightness * 1023);
    warm = (int) ((1 - colorMix) * brightness * 1023);
    if (warm < 0) warm = 0;
    if (warm > 1023) warm = 1023;
    if (cool < 0) cool = 0;
    if (cool > 1023) cool = 1023;
  }
  char sz[24];
  sprintf(sz, "warm: %4d", warm);
  obdWriteString(&obd, 0, 0, 16, sz, FONT_8x8, 0, 0);
  sprintf(sz, "cool: %4d", cool);
  obdWriteString(&obd, 0, 0, 24, sz, FONT_8x8, 0, 0);
  sprintf(sz, "time: %02d:%02d", currentMin / 60, currentMin % 60);
  obdWriteString(&obd, 0, 0, 32, sz, FONT_8x8, 0, 0);
  analogWrite(LED_A_WARM, warm);  // [0..1024)
  analogWrite(LED_A_COOL, cool);  // [0..1024)
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
  pinMode(LED_ONE_MIN_DAY_SIM_PIN, INPUT_PULLUP);
  pinMode(LED_A_WARM, OUTPUT);
  pinMode(LED_A_COOL, OUTPUT);
  analogWriteFreq(2048);
  analogWriteResolution(10 /* bits */);
  analogWrite(LED_A_WARM, 0);  // [0..1024)
  analogWrite(LED_A_COOL, 0);  // [0..1024)
  
  Serial.begin(115200);
  delay(500);

  // init display
  obdI2CInit(&obd, OLED_128x64, -1, 0, 0, 1, -1, -1, -1, 400000L);  // probably can go to 800kHz
  // clear the display buffer and dispaly
  memset(obdBackBuffer, 0x00, sizeof(obdBackBuffer));
  obdSetBackBuffer(&obd, obdBackBuffer);
  obdDumpBuffer(&obd, NULL);

//  // init software PWM
//  Serial.print(F("\nStarting ISR_Modify_PWM on ")); Serial.println(ARDUINO_BOARD);
//  Serial.println(ESP8266_PWM_VERSION);
//  Serial.print(F("CPU Frequency = ")); Serial.print(F_CPU / 1000000); Serial.println(F(" MHz"));
//  // Interval in microsecs
//  if (ITimer.attachInterruptInterval(HW_TIMER_INTERVAL_US, TimerHandler)) {
//    Serial.print(F("Starting ITimer OK"));
//    channelNum = ISR_PWM.setPWM_Period(LED_A_WARM, PWM_Period1, PWM_DutyCycle1);
//  } else {
//    Serial.println(F("Can't set ITimer. Select another freq. or timer"));
//  }
    
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
  digitalWrite(LED_BUILTIN, 1);
  Serial.println("");
  Serial.printf("Connected to %s (%d db)\n", ssid, WiFi.RSSI());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  char szTemp[32];
  sprintf(szTemp, "IP: %s", WiFi.localIP().toString().c_str());
  //  obdWriteString(&obd, 0, 0, 8, szTemp, FONT_6x8, 0, 1);

  // Wait for accurate time info
  Serial.println("Waiting for NTP time update");
  configNptTime(TZ_INFO, NULL);
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
  digitalWrite(LED_BUILTIN, 1);
  Serial.println("");
  Serial.print("Current Time: ");
  char time_output[30];
  strftime(time_output, 30, "%a %Y-%m-%d %T", getNtpTime(0));
  Serial.println(time_output);

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/gif", []() {
    static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });

  server.onNotFound(handleNotFound);

  /////////////////////////////////////////////////////////
  // Hook examples

  server.addHook([](const String & method, const String & url, WiFiClient * client, ESP8266WebServer::ContentTypeFunction contentType) {
    (void)method;      // GET, PUT, ...
    (void)url;         // example: /root/myfile.html
    (void)client;      // the webserver tcp client connection
    (void)contentType; // contentType(".html") => "text/html"
    Serial.printf("A useless web hook has passed\n");
    Serial.printf("(this hook is in 0x%08x area (401x=IRAM 402x=FLASH))\n", esp_get_program_counter());
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  server.addHook([](const String&, const String & url, WiFiClient*, ESP8266WebServer::ContentTypeFunction) {
    if (url.startsWith("/fail")) {
      Serial.printf("An always failing web hook has been triggered\n");
      return ESP8266WebServer::CLIENT_MUST_STOP;
    }
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  server.addHook([](const String&, const String & url, WiFiClient * client, ESP8266WebServer::ContentTypeFunction) {
    if (url.startsWith("/dump")) {
      Serial.printf("The dumper web hook is on the run\n");

      // Here the request is not interpreted, so we cannot for sure
      // swallow the exact amount matching the full request+content,
      // hence the tcp connection cannot be handled anymore by the
      // webserver.
#ifdef STREAMSEND_API
      // we are lucky
      client->sendAll(Serial, 500);
#else
      auto last = millis();
      while ((millis() - last) < 500) {
        char buf[32];
        size_t len = client->read((uint8_t*)buf, sizeof(buf));
        if (len > 0) {
          Serial.printf("(<%d> chars)", (int)len);
          Serial.write(buf, len);
          last = millis();
        }
      }
#endif
      // Two choices: return MUST STOP and webserver will close it
      //                       (we already have the example with '/fail' hook)
      // or                  IS GIVEN and webserver will forget it
      // trying with IS GIVEN and storing it on a dumb WiFiClient.
      // check the client connection: it should not immediately be closed
      // (make another '/dump' one to close the first)
      Serial.printf("\nTelling server to forget this connection\n");
      static WiFiClient forgetme = *client; // stop previous one if present and transfer client refcounter
      return ESP8266WebServer::CLIENT_IS_GIVEN;
    }
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  // Hook examples
  /////////////////////////////////////////////////////////

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
