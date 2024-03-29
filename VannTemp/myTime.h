#include "time.h"
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; //3600;
const int   daylightOffset_sec = 3600;

int timeHour(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return -1;
  }
  return timeinfo.tm_hour;
}
int timeMinute(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return -1;
  }
  return timeinfo.tm_min;
}

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  // Serial.print("Day of week: ");
  // Serial.println(&timeinfo, "%A");
  // Serial.print("Month: ");
  // Serial.println(&timeinfo, "%B");
  // Serial.print("Day of Month: ");
  // Serial.println(&timeinfo, "%d");
  // Serial.print("Year: ");
  // Serial.println(&timeinfo, "%Y");
  // Serial.print("Hour: ");
  // Serial.println(&timeinfo, "%H");
  // Serial.print("Hour (12 hour format): ");
  // Serial.println(&timeinfo, "%I");
  // Serial.print("Minute: ");
  // Serial.println(&timeinfo, "%M");
  // Serial.print("Second: ");
  // Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

void GetTime(){

  if (WiFi.status() == WL_CONNECTED) {
    // Init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
  }
  else
    Serial.println("WiFi not connected.");
}

// void loopTime(){
//   delay(1000);
//   printLocalTime();
// }

