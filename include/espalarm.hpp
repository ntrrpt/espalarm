#include <Arduino.h>

// rtime storage
#include <EEPROM.h>

// uart
#include <SoftwareSerial.h>

// DFPlayer
#include <DFMiniMp3.h>
#include <DfMp3Types.h>
#include <Mp3ChipBase.h>
#include <Mp3ChipIncongruousNoAck.h>
#include <Mp3ChipMH2024K16SS.h>
#include <Mp3ChipOriginal.h>

// brownout detector
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// wifi
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// time sync
#include <NTPClient.h>
#include <WiFiUdp.h>

// url parsing (?)
#include <uri/UriBraces.h>
#include <uri/UriRegex.h>

#define EEPROM_SIZE 8 // sizeof(rtime)

#ifdef DEBUG
#define DP(x) Serial.print(x)
#define DPLN(x) Serial.println(x)
#define DPF(x...) Serial.printf(x)
#else
#define DP(x)
#define DPLN(x)
#define DPF(x...)
#endif

#define WEB_PAGE "\
      <html>\
        <head>\
          <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; overflow: hidden; }\
          </style>\
        </head>\
        <body>\
          <h1><a href='/'>Hello from ESP32!</a></h1>\
          <p>Current time: %02d:%02d:%02d</p>\
          <p>Alarm time: %02d:%02d</p>\
            <form action='/web'>\
              <input type='number' id='hours' name='hours' value=%d>\
              <input type='number' id='minutes' name='minutes' value=%d><br><br>\
              <input type='submit' value='Set alarm'>\
            </form> \
        </body>\
      </html>"

struct hhmm
{
  int hour;   // normal range 0 - 23
  int minute; // normal range 0 - 59
};

hhmm rtime;
int upseconds = 0;   // seconds after boot
int rtime_lock = -1; // does not allow to ring immediately after snooze
int reset_timer = 0;
bool ringing = false;
