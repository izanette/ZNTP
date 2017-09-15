#include <zntp.h>

///////////////////////////////////////
// NTP Settings
ZNTP ntp;
char* ntpServerName           = "time.nist.gov";  // there are lot's of different NTP servers

///////////////////////////////////////
// TimeZoneDB Settings
char* timeZoneDBAPIKey        = "YOUR_API_KEY";   // go to http://timezonedb.com, create a free account and get an API key
char* timeZoneDBZone          = "Europe/London";  // one of the time zones listed here: https://timezonedb.com/time-zones

///////////////////////////////////////
// WiFi Settings
const char* ssid              = "YOUR_SSID";       // your wireless network name (SSID)
const char* password          = "YOUR_PASSWORD";   // your Wi-Fi network password

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  ntp.begin(ntpServerName, timeZoneDBAPIKey, timeZoneDBZone);
}

void loop() {
  ydelay(1000);
    
  if (ntp.isTimeSet()) {
    Serial.print("Time in London is: ");
    ntp.printTime();
  }
  else {
    Serial.println("Time is not set");
    ntp.sync();
  }
}

void ydelay(int milliseconds) {
  unsigned long endTime = millis() + milliseconds;
  while(millis() < endTime)
    yield();
}
