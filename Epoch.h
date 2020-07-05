#pragma once
#include <stdio.h>
#include "time.h"

class Epoch {
  private:
    time_t t;
    struct tm timeinfo;
    const char* wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
    const char* ntp_server = "ntp.nict.jp";
    unsigned long jst_offset = 3600UL * 9; // 日本標準時(+9)
    int ntp_interval = 10800; // デフォルト値:3時間毎に更新
    time_t next_event_epoch = 0;
    
  public:
    Epoch() {}
    Epoch(int ntp_interval_seconds) {
      ntp_interval = ntp_interval_seconds;
    }
    void ntpSync() {
      configTime(jst_offset, 0, ntp_server);
      t = time(NULL);
    }
    time_t get() { return t; }
    int getStep(int max_steps) { return (int)t%max_steps; }
    String getDateString() {
      getLocalTime(&timeinfo);
      char buf[16];
      sprintf(buf, "%04d/%02d/%02d(%s)", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, wd[timeinfo.tm_wday]);
      return String(buf);
    }
    String getTimeString() {
      getLocalTime(&timeinfo);
      char buf[9];
      sprintf(buf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return String(buf);
    }
    void getDebugCharArray(char* buffer, String message) {
      char mesbuf[256];
      message.toCharArray(mesbuf, message.length() + 1);
      getLocalTime(&timeinfo);
      sprintf(buffer,
        "[%04d/%02d/%02d(%s) %02d:%02d:%02d] %s",
        timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, wd[timeinfo.tm_wday],
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, mesbuf
       );
    }
    void clearNextEvent() {
      next_event_epoch = 0;
    }
    time_t setNextEvent(unsigned long after_seconds) {
      next_event_epoch = t + (time_t)after_seconds;
      return next_event_epoch;
    }
    time_t getUntilNextEvent() {
      if (next_event_epoch == 0) return 0;
      return next_event_epoch - t;
    }
    void loop() {
      t = time(NULL);
      if (t % ntp_interval == 0) ntpSync();
    }
};
