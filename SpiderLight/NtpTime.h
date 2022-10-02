#ifndef _NTP_TIME_H
#define _NTP_TIME_H
#include <Time.h>

/**
 * 
 * @param timezoneInfo Timezone info string - see https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
 * @param ntpServerAddress NULLABLE, or NTP server address - see https://www.ntppool.org
 */
// Hawaii Time HAW10
// Alaska Time AKST9AKDT
// Pacific Time  PST8PDT
// Mountain Time MST7MDT
// Mountain Time (Arizona, no DST) MST7
// Central Time  CST6CDT
// Eastern Time  EST5EDT
void configNptTime(const char* timezoneInfo, const char* ntpServerAddress);

/**
 * Requires WiFi connection.
 */
bool syncNtpTime();

const tm* getNtpTime();

#endif
