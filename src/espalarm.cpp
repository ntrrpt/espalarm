#include "espalarm.hpp"

// wifi
const char *ssid = "ssid";
const char *password = "password";

// snooze time (in minutes)
int SNOOZE_TIME = 5;

// leds
int PIN_RED = 27;
int PIN_GREEN = 26;
int PIN_BLUE = 33;

// relay (on/off speakers)
int PIN_RELAY = 32;

// snooze button
int PIN_BUTTON = 13;

// dfplayer rx/tx
int PIN_DFRX = 35;
int PIN_DFTX = 14;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 120000);
//                                                  UTC+0  2 mins update interval

WebServer server(80);

class Mp3Notify;

EspSoftwareSerial::UART swSer1(PIN_DFRX, PIN_DFTX);
typedef DFMiniMp3<EspSoftwareSerial::UART, Mp3Notify> DfMp3;
DfMp3 dfmp3(swSer1);

class Mp3Notify
{
public:
  static void OnPlayFinished([[maybe_unused]] DfMp3 &mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track)
  {
    DP("Play finished for #");
    DPLN(track);
    dfmp3.increaseVolume();
    dfmp3.playGlobalTrack(1);
  }
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char *action)
  {
    if (source & DfMp3_PlaySources_Sd)
      DP("SD Card, ");
    if (source & DfMp3_PlaySources_Usb)
      DP("USB Disk, ");
    if (source & DfMp3_PlaySources_Flash)
      DP("Flash, ");
    DPLN(action);
  }
  static void OnError([[maybe_unused]] DfMp3 &mp3, uint16_t errorCode)
  {
    DPF("Com Error %d\n", errorCode);
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3 &mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3 &mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3 &mp3, DfMp3_PlaySources source)
  {
    PrintlnSourceAction(source, "removed");
  }
};

void setColor(int R, int G, int B)
{
  digitalWrite(PIN_RED, (R == 0) ? LOW : HIGH);
  digitalWrite(PIN_GREEN, (G == 0) ? LOW : HIGH);
  digitalWrite(PIN_BLUE, (B == 0) ? LOW : HIGH);
}

void handleRoot()
{
  char temp[1024];
  snprintf(temp, 1024, WEB_PAGE,
           int(timeClient.getHours()),
           int(timeClient.getMinutes()),
           int(timeClient.getSeconds()),
           rtime.hour, rtime.minute,
           rtime.hour, rtime.minute);

  server.send(200, "text/html", temp);
}

void handleNotFound()
{
  char temp[512];
  String message = "File Not Found\n\n";

  sprintf(temp, "URI: %s\n", server.uri());
  message += temp;

  sprintf(temp, "Method: %s\n", (server.method() == HTTP_GET) ? "GET" : "POST");
  message += temp;

  sprintf(temp, "Arguments: %d\n", server.args());
  message += temp;

  for (uint8_t i = 0; i < server.args(); i++)
  {
    sprintf(temp, "\t%s: %s\n", server.argName(i), server.arg(i));
    message += temp;
  }

  server.send(404, "text/plain", message);
}

void handleApi(bool is_web)
{
  if (!server.hasArg("hours") || !server.hasArg("minutes"))
  {
    handleNotFound();
    return;
  }

  char temp[256];

  String s_hours = server.arg("hours");
  String s_minutes = server.arg("minutes");

  int hours = atoi(s_hours.c_str());
  int minutes = atoi(s_minutes.c_str());

  if (hours > 59)
  {
    sprintf(temp, "Alarm time: %d:%02d\n", rtime.hour, rtime.minute);
    server.send(200, "text/plain", temp);
    return;
  }

  if (minutes > 59)
  {
    reset_timer = upseconds + 4;
    server.send(200, "text/plain", "reset in 4 seconds...\n\n");
    return;
  }

  rtime = {hours, minutes};
  EEPROM.put(0, rtime);
  EEPROM.commit();

  setColor(0, 0, 1);

  if (is_web)
  {
    handleRoot();
    return;
  }

  String message = "alarm enabled!\n\n";

  sprintf(temp, "Current time: %d:%02d:%02d\n", timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
  message += temp;

  sprintf(temp, "Alarm time: %d:%02d\n", hours, minutes);
  message += temp;

  server.send(200, "text/plain", message);
}

void setup(void)
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);

  rtime = EEPROM.get(0, rtime);
  DPF("EEPROM => %d:%d \n", rtime.hour, rtime.minute);

  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.waitForConnectResult();
  DPLN(WiFi.localIP());

  if (MDNS.begin("esp32"))
  {
    DPLN("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on(UriBraces("/web"), []()
            { handleApi(true); });

  server.on(UriBraces("/api"), []()
            { handleApi(false); });

  server.onNotFound(handleNotFound);

  timeClient.begin();

  server.begin();
  DPLN("HTTP server started");

  dfmp3.begin();
  DPLN("DFplayer started");

  // relay testing
  digitalWrite(PIN_RELAY, HIGH);
  delay(1000);
  digitalWrite(PIN_RELAY, LOW);

  setColor(0, 0, 1);
  if (rtime.minute < 0 ||
      rtime.minute > 59 ||
      rtime.hour < 0 ||
      rtime.hour > 59)
  {
    setColor(1, 0, 0);
  }
}

void loop(void)
{
  server.handleClient();
  timeClient.update();
  dfmp3.loop();

  int current_minute = int(timeClient.getMinutes());
  int current_hour = int(timeClient.getHours());
  int sec = millis() / 1000;

  if (upseconds != sec)
  {
    upseconds = sec;
  }

  // led blinking
  if (ringing)
  {
    int r = (millis() % 200 < 100) ? HIGH : LOW;
    setColor(r, r, r);
  }

  // ring
  if (current_minute != rtime_lock &&
      current_minute == rtime.minute &&
      current_hour == rtime.hour)
  {
    rtime_lock = current_minute;
    ringing = true;

    digitalWrite(PIN_RELAY, HIGH);
    dfmp3.setVolume(20);
    dfmp3.playGlobalTrack(1);
  }

  // snooze
  if (rtime_lock > -1)
  {
    if (!ringing && rtime_lock != current_minute)
    {
      rtime_lock == -1;
    }

    if (ringing && digitalRead(PIN_BUTTON) == HIGH)
    {
      rtime = {current_hour, current_minute + SNOOZE_TIME};
      EEPROM.put(0, rtime);
      EEPROM.commit();

      if (rtime.minute > 59)
      {
        rtime.minute -= 60;
        rtime.hour += 1;
        if (rtime.hour > 23)
        {
          rtime.hour -= 24;
        }
      }

      DPF("%d:%d => %d:%d, snooze for %d minutes\n",
          current_hour, current_minute,
          rtime.hour, rtime.minute,
          SNOOZE_TIME);

      ringing = false;
      setColor(0, 0, 1);
      dfmp3.stop();
      digitalWrite(PIN_RELAY, LOW);
    }
  }

  if (reset_timer > 0 && reset_timer < upseconds)
  {
    EEPROM.put(0, hhmm{-1, -1});
    EEPROM.commit();

    dfmp3.reset();
    ESP.restart();
  }
}
