// Libraries to get time from NTP Server
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "password.h"

#include <M5Core2.h>

void setup() {
  M5.begin(true, true, true, true);

  // put your setup code here, to run once:
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");
  configTime(0, 0, "pool.ntp.org");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  RTC_TimeTypeDef rtctime;
  RTC_DateTypeDef rtcdate;

  rtctime.Seconds = timeinfo.tm_sec;
  rtctime.Minutes = timeinfo.tm_min;
  rtctime.Hours = timeinfo.tm_hour;
  rtcdate.Year = timeinfo.tm_year + 1900;
  rtcdate.Month = timeinfo.tm_mon + 1;
  rtcdate.Date = timeinfo.tm_mday;

  M5.Rtc.SetData(&rtcdate);
  M5.Rtc.SetTime(&rtctime);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

}

void loop() {
  // put your main code here, to run repeatedly:

}
