#include "NtpTime.h"
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif
#include <Time.h>

/**
  Definition of struct tm:
  Member  Type  Meaning Range
  tm_sec  int seconds after the minute  0-61*
  tm_min  int minutes after the hour  0-59
  tm_hour int hours since midnight  0-23
  tm_mday int day of the month  1-31
  tm_mon  int months since January  0-11
  tm_year int years since 1900
  tm_wday int days since Sunday 0-6
  tm_yday int days since January 1  0-365
  tm_isdst  int Daylight Saving Time flag
*/

#define NTP_SYNC_RATE_SEC (3600 * 100)

tm timeinfo;
time_t lastSyncNtpTimeSec(0);
uint32_t lastSyncMillis(0);


void configNptTime(const char* timezoneInfo, const char* ntpServerAddress) {
  configTime(0, 0, (ntpServerAddress != NULL ? ntpServerAddress : "pool.ntp.org"));
  setenv("TZ", timezoneInfo, 1);
}


bool syncNtpTime() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  uint32_t start = millis();
  time_t now;
  time(&now);  // Calls NTP time servers
  if (now > 1000000) {
    lastSyncMillis = ((millis() - start) / 2) + start;
    lastSyncNtpTimeSec = now;
    return true;
  }
  return false;
}


const tm* getNtpTime() {
  uint32_t offsetSec = (millis() - lastSyncMillis) / 1000ul;
  if (lastSyncMillis == 0 || (NTP_SYNC_RATE_SEC != 0 && offsetSec >= NTP_SYNC_RATE_SEC)) {
    syncNtpTime();
  }
  time_t now = lastSyncNtpTimeSec + offsetSec;
  return localtime(&now);
}
