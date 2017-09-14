#ifndef ZNTP_H
#define ZNTP_H

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

class ZNTP
{
public:
    ZNTP();
    void begin(unsigned long gmtOffset);
    void begin(String NTPServerName = "", unsigned long gmtOffset = 0);
    void begin(String timeZoneDBAPIKey, String timeZoneDBZone);
    void begin(String NTPServerName, String timeZoneDBAPIKey, String timeZoneDBZone);
    
    void            sync(bool force = false);
    void            printTime();
    bool            isTimeSet();
    
    void            setUpdateInterval(unsigned long value) { m_NTPUpdateInterval = value; }
    
private: // attributes
    ///////////////////////////////////////
    // WiFi Settings
    // A UDP instance to let us send and receive packets over UDP
    WiFiUDP         m_udp;

    ///////////////////////////////////////
    // NTP Settings
    String          m_NTPServerName;
    unsigned long   m_NTPUpdateInterval;
    unsigned long   m_lastNTPUpdate;
    bool            m_ntpTimeSet;
    long            m_gmtOffset;
    bool            m_gmtOffsetSet;

    ///////////////////////////////////////
    // TimeZoneDB Settings
    String          m_timeZoneDBAPIKey;
    String          m_timeZoneDBZone;
    
private: // methods
    void            initialize();
    void            ydelay(int milliseconds); // yield + delay
    bool            waitForClient(WiFiClient& client);
    bool            noDataFromService(WiFiClient& client, const char* err);
    String          getJsonField(const String& json, const String& field);
    bool            connectWiFi();
    bool            connectUDP();
    unsigned long   sendNTPpacket(IPAddress& address, byte packetBuffer[]);
    bool            updateGmtOffset();
    bool            getTimeZoneDBStatus(WiFiClient& client);
    void            printTime(time_t t);
};

#endif // ZNTP_H