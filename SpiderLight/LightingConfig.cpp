#include "LightingConfig.h"
#include <Time.h>
#include "ArduinoJson.h"
#include "CRC.h"

#define EEPROM_BYTE_FORMAT_VERSION 1

// float interpellate(float a, float b, float p) {
//   return a + p * (b - a);
// }
// float interpellateInv(int a, int b, int p) {
//   int v = b - a;
//   float w = p - a;
//   return w / v;
// }

float ChannelConfig::calcColorRatio(float colorTemp) {
  if (coolTemp == warmTemp) return 0.5;
  float p((coolTemp - colorTemp) / (coolTemp - warmTemp));
  if (p < 0) p = 0;
  if (p > 1) p = 1;    
  return !swapWarmCoolSignals ? p : 1 - p;
}

void LightingConfig::addScheduleEntry(int time, int colorTemp, int8_t brightness) {
  schedule.push_back(ScheduleEntry(time, colorTemp, brightness));
}

ChannelOutput LightingConfig::calcOutputs(const tm* now) {
  // TODO: shift DST out of the light schedule
  const int currentMin(now->tm_hour * 60 + now->tm_min);  // realtime
  // const int currentMin(((now->tm_min % 5) * 60 + now->tm_sec)* 5);  // simulate a day over 5 min (ok, so it's a 25hr day simulated - meh)
  // const int currentMin(now->tm_sec * 24);  // simulate a day over one min
  if (schedule.size() < 2 || currentMin < schedule[0].time || currentMin >= schedule[schedule.size() - 1].time) {
    return {currentMin, 0, 0, 0, 0};
  }

  //const int32_t currentSec(currentMin * 60 + now->tm_sec);

  ScheduleEntry* se1(&schedule[0]);
  ScheduleEntry* se2;
  for (int i = 1; i < schedule.size(); i++) {
    se2 = &schedule[i];
    if (currentMin >= se1->time && currentMin < se2->time) {
      break;
    }
    se1 = se2;
  }
  
  const float p((currentMin - se1->time) / (float)(se2->time - se1->time));
  const float brightness((se1->brightness + ((se2->brightness - se1->brightness) * p)) / 100.0); // `brightness` is in percent [0..100]
  if (brightness < 0.001) {
    return {currentMin, 0, 0, 0, 0};
  }
  const float colorTemp(se1->colorTemp + ((se2->colorTemp - se1->colorTemp) * p));
  const float colorMixA(channelConfigA.calcColorRatio(colorTemp));
  const float colorMixB(channelConfigB.calcColorRatio(colorTemp));
  const float brightA(channelConfigA.enabled ? brightness * (channelConfigA.maxBrightness / 100.0) : 0);
  const float brightB(channelConfigB.enabled ? brightness * (channelConfigB.maxBrightness / 100.0) : 0);
  return {currentMin,
   colorMixA * brightA, (1 - colorMixA) * brightA,
   colorMixB * brightB, (1 - colorMixB) * brightB};
}

DynamicJsonDocument LightingConfig::toJson() {
  DynamicJsonDocument doc(2048);
  doc["timezone"] = timezone;

  JsonObject channel_a = doc.createNestedObject("channel_a");
  channel_a["enabled"] = channelConfigA.enabled;
  channel_a["max_brightness"] = channelConfigA.maxBrightness;
  channel_a["swap_warm_cool_sig"] = channelConfigA.swapWarmCoolSignals;
  channel_a["warm_temp"] = channelConfigA.warmTemp;
  channel_a["cool_temp"] = channelConfigA.coolTemp;
  
  JsonObject channel_b = doc.createNestedObject("channel_b");
  channel_b["enabled"] = channelConfigB.enabled;
  channel_b["max_brightness"] = channelConfigB.maxBrightness;
  channel_b["swap_warm_cool_sig"] = channelConfigB.swapWarmCoolSignals;
  channel_b["warm_temp"] = channelConfigB.warmTemp;
  channel_b["cool_temp"] = channelConfigB.coolTemp;

  JsonArray scheduleArr = doc.createNestedArray("schedule");
  for (auto se : schedule) {
    JsonArray seArr = scheduleArr.createNestedArray();
    seArr.add(se.time);
    seArr.add(se.colorTemp);
    seArr.add(se.brightness);
  }

  return doc;
}

StatusOrError LightingConfig::fromJson(DynamicJsonDocument& obj) {
  if (!obj.containsKey("channel_a")) return {false, "missing channel_a"};
  if (!obj.containsKey("channel_b")) return {false, "missing channel_b"};
  if (!obj.containsKey("schedule")) return {false, "missing schedule"};
  if (obj.containsKey("timezone")) {
    const char* tz = obj["timezone"].as<char*>();
    if (strlen(tz) > 64) {
      return {false, "timezone string was too long, max length 64"};   
    }
    timezone.assign(tz);
  } else {
     return {false, "missing timezone"};
  }
  
  JsonObject channel_a = obj["channel_a"];
  channelConfigA.enabled = channel_a["enabled"];
  channelConfigA.swapWarmCoolSignals = channel_a["swap_warm_cool_sig"];
  channelConfigA.maxBrightness = channel_a["max_brightness"];
  channelConfigA.warmTemp = channel_a["warm_temp"];
  channelConfigA.coolTemp = channel_a["cool_temp"];

  JsonObject channel_b = obj["channel_b"];
  channelConfigB.enabled = channel_b["enabled"];
  channelConfigB.swapWarmCoolSignals = channel_b["swap_warm_cool_sig"];
  channelConfigB.maxBrightness = channel_b["max_brightness"];
  channelConfigB.warmTemp = channel_b["warm_temp"];
  channelConfigB.coolTemp = channel_b["cool_temp"];

  JsonArray scheduleArr = obj["schedule"];
  for (JsonVariant sched : scheduleArr) {
    JsonArray schedVals = sched.as<JsonArray>();
    if (schedVals.size() != 3) return {false, "invalid schedule entry"};    
    addScheduleEntry(schedVals[0], schedVals[1], schedVals[2]);
  }
  return {true, "OK"};
}

LightingConfigEepromHeader LightingConfig::calcEepromDataHeader() {
  int size = 6 * 2;  // channel configs (bools are bit mapped)
  size += timezone.length() + 1;  // +1 to account for a byte to record string length
  size += schedule.size() * 5 + 1;  // +1 for entry count
  size += 2;  // CRC16
  return {EEPROM_BYTE_FORMAT_VERSION, size};
}

int LightingConfig::serialize(byte* buff, uint16_t& crcOut) {
  int i(0);
  buff[i++] = (int8_t) timezone.length();
  for(char& c : timezone) {
    buff[i++] = (int8_t) c;    
  }
  buff[i++] = channelConfigA.enabled | (channelConfigA.swapWarmCoolSignals << 1);
  buff[i++] = channelConfigA.maxBrightness;
  buff[i++] = (int8_t) channelConfigA.warmTemp;
  buff[i++] = (int8_t) (channelConfigA.warmTemp >> 8);
  buff[i++] = (int8_t) channelConfigA.coolTemp;
  buff[i++] = (int8_t) (channelConfigA.coolTemp >> 8);

  buff[i++] = channelConfigB.enabled | (channelConfigB.swapWarmCoolSignals << 1);
  buff[i++] = channelConfigB.maxBrightness;
  buff[i++] = (int8_t) channelConfigB.warmTemp;
  buff[i++] = (int8_t) (channelConfigB.warmTemp >> 8);
  buff[i++] = (int8_t) channelConfigB.coolTemp;
  buff[i++] = (int8_t) (channelConfigB.coolTemp >> 8);

  buff[i++] = schedule.size();
  for (auto se : schedule) {
    buff[i++] = (int8_t) se.time;
    buff[i++] = (int8_t) (se.time >> 8);
    buff[i++] = (int8_t) se.colorTemp;
    buff[i++] = (int8_t) (se.colorTemp >> 8);
    buff[i++] = se.brightness;
  }
  crcOut = crc16(buff, i);
  buff[i++] = (int8_t) crcOut;
  buff[i++] = (int8_t) (crcOut >> 8);
  return i;
}

/* static */ StatusOrError LightingConfig::validateHeader(const LightingConfigEepromHeader& header) {
  if (!(header.version > 0 && header.version <= EEPROM_BYTE_FORMAT_VERSION)) {
    char sz[32];
    sprintf(sz, "Bad data version %d", header.version);
    return {false, sz};
  }
  if (header.size > 120) {
    char sz[32];
    sprintf(sz, "Bad size %d", header.size);
    return {false, sz};
  }
  return {true, "OK"};
}

StatusOrError LightingConfig::deserialize(const LightingConfigEepromHeader& header, byte* data) {
  StatusOrError soe = LightingConfig::validateHeader(header);
  if (!soe.ok) return soe;
  if (header.version == 1) {
    return deserializeV1(header, data);
  }
  return {false, "Unimplemented data version"};
}

StatusOrError LightingConfig::deserializeV1(const LightingConfigEepromHeader& header, byte* data) {
  if (header.version != 1) return {false, "Expected data version(1)"};
  int i(0);
  int k = data[i++];
  char tz[k+1];
  tz[k] = 0;
  for (int j = 0; j < k; j++) {
    tz[j] = (char) data[i++];
  }
  timezone.assign(tz);

  k = data[i++];
  channelConfigA.enabled = k & 1;
  channelConfigA.swapWarmCoolSignals = (k & 2) >> 1;
  channelConfigA.maxBrightness = data[i++];
  channelConfigA.warmTemp = data[i++] | (data[i++] << 8);
  channelConfigA.coolTemp = data[i++] | (data[i++] << 8);

  k = data[i++];
  channelConfigB.enabled = k & 1;
  channelConfigB.swapWarmCoolSignals = (k & 2) >> 1;
  channelConfigB.maxBrightness = data[i++];
  channelConfigB.warmTemp = data[i++] | (data[i++] << 8);
  channelConfigB.coolTemp = data[i++] | (data[i++] << 8);

  schedule.clear();
  k = data[i++];
  for (int j = 0; j < k; j++) {
    addScheduleEntry(
      data[i++] | (data[i++] << 8),
      data[i++] | (data[i++] << 8),
      data[i++]
    );
  }
  // check CRC
  uint16_t actualCrc = crc16(data, i);
  uint16_t expectCrc = data[i++] | (data[i++] << 8);
  char sz[64];
  sz[0] = 0;
  if (actualCrc == expectCrc) {
    sprintf(sz, "OK - CRC(0x%04X)", actualCrc);
    return {true, sz};
  }
  sprintf(sz, "CRC check failed - expected 0x%04X != 0x%04X actual", expectCrc, actualCrc);
  return {false, sz};
}