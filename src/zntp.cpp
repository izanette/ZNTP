#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include "zntp.h"

#define ZNTP_DEBUG_SERIAL
#define UDP_LOCAL_PORT         2390
#define NTP_PACKET_SIZE        48                      // NTP time stamp is in the first 48 bytes of the message
#define TIMEZONEDB_SERVER      "api.timezonedb.com"


#ifdef ZNTP_DEBUG_SERIAL
    #define ZNTP_PRINT(x)          Serial.print(x)
    #define ZNTP_PRINTF(...)       Serial.printf(__VA_ARGS__)
    #define ZNTP_PRINTLN(x)        Serial.println(x)
#else
    #define ZNTP_PRINT(x)
    #define ZNTP_PRINTF(...)
    #define ZNTP_PRINTLN(x)
#endif

ZNTP::ZNTP()
{
    initialize();
}

void ZNTP::begin(unsigned long gmtOffset)
{
    m_gmtOffset    = gmtOffset;
    m_gmtOffsetSet = true;
    sync();
}

void ZNTP::begin(String NTPServerName /*= ""*/, unsigned long gmtOffset /*= 0*/)
{
    m_NTPServerName = NTPServerName;
    m_gmtOffset     = gmtOffset;
    m_gmtOffsetSet  = true;
    sync();
}

void ZNTP::begin(String timeZoneDBAPIKey, String timeZoneDBZone)
{
    m_timeZoneDBAPIKey = timeZoneDBAPIKey;
    m_timeZoneDBZone   = timeZoneDBZone;
    sync();
}

void ZNTP::begin(String NTPServerName, String timeZoneDBAPIKey, String timeZoneDBZone)
{
    m_NTPServerName    = NTPServerName;
    m_timeZoneDBAPIKey = timeZoneDBAPIKey;
    m_timeZoneDBZone   = timeZoneDBZone;
    sync();
}

void ZNTP::initialize()
{
    m_NTPServerName                   = "time.nist.gov";
    m_NTPUpdateInterval               = 240 * 60000;
    m_lastNTPUpdate                   = 0;
    m_ntpTimeSet                      = false;
    m_gmtOffset                       = 0;
    m_gmtOffsetSet                    = false;
    m_timeZoneDBAPIKey                = "";
    m_timeZoneDBZone                  = "";
}

void ZNTP::ydelay(int milliseconds)
{
    unsigned long endTime = millis() + milliseconds;
    while(millis() < endTime)
    {
        yield();
    }
}

bool ZNTP::connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return true;
    
    ZNTP_PRINT(F("Wifi: Trying to connect"));
    int tries = 0;
    while (tries < 10 && WiFi.status() != WL_CONNECTED)
    {
        ydelay(500);
        ZNTP_PRINT(".");
        tries++;
    }
    ZNTP_PRINTLN("");
    
    if (WiFi.status() != WL_CONNECTED)
        return false;
    
    ZNTP_PRINTLN(F("WiFi connected"));
    ZNTP_PRINTF("SSID: %s\n", WiFi.SSID().c_str());
    ZNTP_PRINT(F("IP address: "));
    ZNTP_PRINTLN(WiFi.localIP());

    return true;
}

bool ZNTP::connectUDP()
{
    if (!connectWiFi())
        return false;
    
    if (m_udp)
        return true;
    
    ZNTP_PRINTLN(F("Starting UDP"));
    m_udp.begin(UDP_LOCAL_PORT);
    ZNTP_PRINTF("Local port: %d\n", m_udp.localPort());
    
    return true;
}

void ZNTP::sync(bool force /*= false*/)
{
    if (!force && isTimeSet() && millis() - m_lastNTPUpdate < m_NTPUpdateInterval)
        return;
    
    if (!connectUDP())
        return;

    if (!updateGmtOffset())
        return;

    ZNTP_PRINTLN(F("== SYNC TIME NTP ============="));

    //get a random server from the pool
    IPAddress timeServerIP;
    WiFi.hostByName(m_NTPServerName.c_str(), timeServerIP); 

    // buffer to hold incoming and outgoing packets
    byte packetBuffer[NTP_PACKET_SIZE];
    sendNTPpacket(timeServerIP, packetBuffer); // send an NTP packet to a time server
    // wait to see if a reply is available
    ydelay(1000);

    int cb = m_udp.parsePacket();
    if (!cb)
    {
        ZNTP_PRINTLN(F("no packet yet"));
        if (m_ntpTimeSet)
            m_lastNTPUpdate += 5000; // wait 5s to try again
        else
            ydelay(5000);
        return;
    }

    ZNTP_PRINT(F("packet received, length="));
    ZNTP_PRINTLN(cb);
    // We've received a packet, read the data from it
    m_udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    ZNTP_PRINT("Seconds since Jan 1 1900 = " );
    ZNTP_PRINTLN(secsSince1900);

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;

    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    ZNTP_PRINT(F("  The UTC time is "));
    printTime(epoch);
    epoch += m_gmtOffset;
    ZNTP_PRINT(F("The local time is "));
    printTime(epoch);

    // print Unix time:
    ZNTP_PRINT(F("Unix time = "));
    ZNTP_PRINTLN(epoch);

    setTime(epoch);

    m_lastNTPUpdate = millis();
    m_ntpTimeSet = true;
}

bool ZNTP::isTimeSet()
{
    return m_ntpTimeSet && m_gmtOffsetSet;
}

void ZNTP::printTime()
{
    printTime(now());
}

void ZNTP::printTime(time_t t)
{
    ZNTP_PRINTF("%02d:%02d:%02d %02d/%02d/%d\n", hour(t), minute(t), second(t), day(t), month(t), year(t));
}

// send an NTP request to the time server at the given address
unsigned long ZNTP::sendNTPpacket(IPAddress& address, byte packetBuffer[])
{
    ZNTP_PRINTLN(F("== SENDING NTP PACKET ========"));
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0]   = 0b11100011;   // LI, Version, Mode
    packetBuffer[1]   = 0;     // Stratum, or type of clock
    packetBuffer[2]   = 6;     // Polling Interval
    packetBuffer[3]   = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    m_udp.beginPacket(address, 123); //NTP requests are to port 123
    m_udp.write(packetBuffer, NTP_PACKET_SIZE);
    m_udp.endPacket();
}

// access the TimeZoneDB webservice and determines the time offset for Sao Paulo
// http://api.timezonedb.com/v2/get-time-zone?key=18DEADBEEF&format=json&by=zone&zone=America/Sao_Paulo
bool ZNTP::updateGmtOffset()
{
    if (m_timeZoneDBAPIKey == "" || m_timeZoneDBZone == "")
        return m_gmtOffsetSet;
    
    ZNTP_PRINTLN(F("== GMT OFFSET ================"));
    m_gmtOffsetSet = false;
    
    String url("/v2/get-time-zone?key=");
    url += m_timeZoneDBAPIKey;
    url += "&format=json&by=zone&zone=";
    url += m_timeZoneDBZone;

    //ZNTP_PRINTF("timeZone URL: %s\n", url.c_str());
    WiFiClient client;
    if (!client.connect(TIMEZONEDB_SERVER, 80)) 
    {
        ZNTP_PRINTLN(F("connection failed!"));
        return m_gmtOffsetSet;
    }
    
    client.print(
        String("GET ") + url + " HTTP/1.1\r\n" + 
        "Host:" + TIMEZONEDB_SERVER + "\r\n" +
        "Connection: close\r\n\r\n");
    
    if (!waitForClient(client))
        return noDataFromService(client, "Unable to connect to the server");
    
    if (!getTimeZoneDBStatus(client)) // JSON packet is avablable
        return noDataFromService(client, "Unable to obtain the time zone information");
    
    if (!waitForClient(client))
        return noDataFromService(client, "Server is not available");
    
    while (client.available())
    {
        yield();

        String response = client.readStringUntil('\n');
        //ZNTP_PRINTF("received response: '%s'\n", response.c_str());
        
        String value = getJsonField(response, "status");
        if (value.length() == 0)
            continue;
        
        if (value != "\"OK\"")
            return noDataFromService(client, "Server doesn't recognize the API Key or Time Zone");
        
        value = getJsonField(response, "gmtOffset");
        if (value == "")
            return noDataFromService(client, "Invalid response from the server");
            
        m_gmtOffset = value.toInt();
        m_gmtOffsetSet = true;

        ZNTP_PRINTF("GMT OFFSET: %d\n", m_gmtOffset);
        break;
    }
    
    client.stop();
    return m_gmtOffsetSet;
}

bool ZNTP::waitForClient(WiFiClient& client)
{
    int count = 0;
    // wait for an answer from the webservice
    while(!client.available() && count < 100)
    {
        ydelay(200);
        count++;
        //if (count % 10 == 0)
        //  ZNTP_PRINTF("Wait %d\n", count++);
    }

    return client.available();
}

bool ZNTP::noDataFromService(WiFiClient& client, const char* err)
{
    ZNTP_PRINTF("ERROR: No data from Service - %s\n", err);
    client.stop();
    return false;
}

String ZNTP::getJsonField(const String& json, const String& field)
{
    String result("");
    String field2("\"");
    field2 += field;
    field2 += "\"";
    int len = field2.length();
    int startField = json.indexOf(field2);
    if (startField < 0)
        return result;

    // add the size of the field name 
    // and 1 because there is a ":" after it
    startField += len + 1;
    
    int endField = json.indexOf(",", startField);
    if (endField < 0)
        endField = json.indexOf("}", startField);
    if (endField < 0)
        return result;

    result = json.substring(startField, endField);
    
    return result;
}

bool ZNTP::getTimeZoneDBStatus(WiFiClient& client)
{
    bool result = false;
    String response;

    response = client.readStringUntil('\n');
    ZNTP_PRINTF("statusline: %s\n", response.c_str());

    int separatorPosition = response.indexOf("HTTP/1.1");

    if (separatorPosition >= 0 && response.substring(9, 12) == "200") 
        result = true;
    
    return  result;
}


