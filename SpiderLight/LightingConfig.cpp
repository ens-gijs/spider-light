#include "LightingConfig.h"
#include <Time.h>
#include "ArduinoJson.h"

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
  float p((coolTemp - colorTemp) / (float)(coolTemp - warmTemp));
  if (p < 0) p = 0;
  if (p > 1) p = 1;    
  return !swapWarmCoolSignals ? p : 1 - p;
}

void LightingConfig::addScheduleEntry(int time, int colorTemp, float brightness) {
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
  const float brightA(brightness * (channelConfigA.maxBrightness / 100));
  const float brightB(brightness * (channelConfigB.maxBrightness / 100));
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

FromJsonResult LightingConfig::fromJson(DynamicJsonDocument& obj) {
  // if (!obj.containsKey("channel_a")) return {false, "missing channel_a"};
  // if (!obj.containsKey("channel_b")) return {false, "missing channel_b"};
  // if (!obj.containsKey("schedule")) return {false, "missing schedule"};
  if (obj.containsKey("timezone")) timezone.assign(obj["timezone"].as<char*>());
  
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
  return {true, ""};
}