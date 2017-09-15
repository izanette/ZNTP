#include <zntp.h>

///////////////////////////////////////
// NTP Settings
ZNTP ntp;

///////////////////////////////////////
// TimeZoneDB Settings
const unsigned int gmtOffset  = -5 * 3600;    // GMT offset for the city of New York (in seconds)

///////////////////////////////////////
// WiFi Settings
const char* ssid              = "YOUR_SSID";       // your wireless network name (SSID)
const char* password          = "YOUR_PASSWORD";   // your Wi-Fi network password

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  ntp.begin(gmtOffset);
}

void loop() {
  ydelay(1000);
    
  if (ntp.isTimeSet()) {
    Serial.print("Time in New York is: ");
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
