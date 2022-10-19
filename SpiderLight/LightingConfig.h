#ifndef _LIGHTING_CONFIG_H
#define _LIGHTING_CONFIG_H
#include <vector>
#include <string>
#include <Time.h>
#include "ArduinoJson.h"

typedef struct {
  int currentMin;
  float pinA1;
  float pinA2;
  float pinB1;
  float pinB2;
} ChannelOutput;

typedef struct {
  bool ok;
  std::string error;
} FromJsonResult;

class ScheduleEntry {
public:
  /** minutes since midnight */
  int time;
  int colorTemp;
  /** percent [0..100] */
  float brightness;

  ScheduleEntry(int time, int colorTemp, float brightness):
    time(time),
    colorTemp(colorTemp),
    brightness(brightness) {}
};

class ChannelConfig {
public:
  bool enabled;
  bool swapWarmCoolSignals;
  /** percent [0..100] */
  float maxBrightness;
  int warmTemp;
  int coolTemp;

  ChannelConfig():
    enabled(true),
    swapWarmCoolSignals(false),
    maxBrightness(100),
    warmTemp(2700),
    coolTemp(6500){}
  
  float calcColorRatio(float colorTemp);
};

class LightingConfig {
public:
  std::string timezone;
  ChannelConfig channelConfigA;
  ChannelConfig channelConfigB;
  std::vector<ScheduleEntry> schedule;

  LightingConfig(): timezone("EST5EDT") {}
  void addScheduleEntry(int time, int colorTemp, float brightness);
  ChannelOutput calcOutputs(const tm* now);
  DynamicJsonDocument toJson();
  FromJsonResult fromJson(DynamicJsonDocument& obj);
};
#endif