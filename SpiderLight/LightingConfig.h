#ifndef _LIGHTING_CONFIG_H
#define _LIGHTING_CONFIG_H
#include <vector>
#include <string>
#include <Time.h>
#include "ArduinoJson.h"

typedef struct {
  /** minutes since midnight */
  const int currentMin;
  // pin's are in range [0..1]
  const float pinA1;
  const float pinA2;
  const float pinB1;
  const float pinB2;
} ChannelOutput;

typedef struct {
  const bool ok;
  const std::string error;
} StatusOrError;

typedef struct {
  uint16_t version;
  uint16_t size;
} LightingConfigEepromHeader;

class ScheduleEntry {
public:
  /** minutes since midnight */
  int time;
  int colorTemp;
  /** percent [0..100] */
  int8_t brightness;

  ScheduleEntry(int time, int colorTemp, int8_t brightness):
    time(time),
    colorTemp(colorTemp),
    brightness(brightness) {}
};

class ChannelConfig {
public:
  bool enabled;
  bool swapWarmCoolSignals;
  /** percent [0..100] */
  int8_t maxBrightness;
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

  static StatusOrError validateHeader(const LightingConfigEepromHeader& header);
  LightingConfigEepromHeader calcEepromDataHeader();
  /** returns size written to buf */
  int serialize(byte* buff, uint16_t& crcOut);
  StatusOrError deserialize(const LightingConfigEepromHeader& header, byte* data);
  
  LightingConfig(): timezone("EST5EDT") {}
  void addScheduleEntry(int time, int colorTemp, int8_t brightness);
  ChannelOutput calcOutputs(const tm* now);
  DynamicJsonDocument toJson();
  StatusOrError fromJson(DynamicJsonDocument& obj);
private:
  StatusOrError deserializeV1(const LightingConfigEepromHeader& header, byte* data);
};
#endif