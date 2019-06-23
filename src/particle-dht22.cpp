
#include "ArduinoJson.h"
#include "Adafruit_DHT.h"
#include "DS3231_Simple.h"
#include "Particle.h"
#include <time.h>

SYSTEM_MODE(AUTOMATIC);

#define DHT_PIN D2
#define DHT_TYPE 22
#define PUBLISH_NAMESPACE "worm"
#define PUBLISH_DEVICE_NAME "mindflayer"
#define DEFAULT_DELAY 900000
#define DEFAULT_ENABLE true
#define DEFAULT_DEEP_SLEEP false
#define FIRMWARE_VERSION 6
#define FIRMWARE_NAME "particle-dht22"
#define STARTUP_DELAY 1000

struct Config {
  uint8_t version;
  unsigned int delayMs;
  bool enabled;
  bool deepSleep;
};

size_t serializeConfig();
void saveConfig();
void firmwareCheck();
void resetConfig();
int enable(String ignored);
int disable(String ignored);
int enableDeepSleep(String ignored);
int disableDeepSleep(String ignored);
int setDelay(String ms);
void publish(String topic, String data);
void readDHT();
void adjustRTC();

DS3231_Simple rtc;
Config defaultConfig = {FIRMWARE_VERSION, DEFAULT_DELAY, DEFAULT_ENABLE,
                        DEFAULT_DEEP_SLEEP};
Config config;
DynamicJsonDocument doc(JSON_OBJECT_SIZE(3));
char configJson[128];
DHT dht(DHT_PIN, DHT_TYPE);

SerialLogHandler logHandler(LOG_LEVEL_WARN, {{"app", LOG_LEVEL_ALL}});

unsigned long prevMillis = 0;

void setup() {
  Log.info("%s on Device OS v%s", FIRMWARE_NAME, System.version().c_str());
  Log.trace("Syncing time");
  Particle.syncTime();
  waitUntil(Particle.syncTimeDone);
  Log.trace("Checking if firmware is up-to-date");
  firmwareCheck();

  Particle.function("enable", enable);
  Particle.function("disable", disable);
  Particle.function("enableDeepSleep", enableDeepSleep);
  Particle.function("disableDeepSleep", disableDeepSleep);
  Particle.function("setDelay", setDelay);
  Particle.variable("config", configJson);

  Log.info("Config: %s", configJson);

  dht.begin();
  rtc.begin();
  adjustRTC();
  rtc.disableAlarms();
  delay(STARTUP_DELAY);
  Log.trace("Performing initial read");
  readDHT();
  prevMillis = millis();
}

time_t timestamp(DateTime dt) {
  struct tm t = {.tm_sec = dt.Second,
                 .tm_min = dt.Minute,
                 .tm_hour = dt.Hour,
                 .tm_mday = dt.Day,
                 .tm_mon = dt.Month - 1,
                 .tm_year = dt.Year + 100};
  return mktime(&t);
}

void adjustRTC() {
  DateTime deviceNow = {.Second = uint8_t(Time.second()),
                        .Minute = uint8_t(Time.minute()),
                        .Hour = uint8_t(Time.hour()),
                        .Dow = uint8_t(Time.weekday()),
                        .Day = uint8_t(Time.day()),
                        .Month = uint8_t(Time.month()),
                        .Year = uint8_t(Time.year() - 2000)};
  rtc.write(deviceNow);
  DateTime now = rtc.read();
  Log.info("Set RTC time to %s",
           Time.format(timestamp(now), TIME_FORMAT_ISO8601_FULL).c_str());
}

void setRTCAlarm() {}

void loop() {
  if (config.enabled) {
    if (config.deepSleep) {
      rtc.setAlarm({DS3231_Simple::ALARM_EVERY_SECOND);
        Log.info("Sleeping for %dms", config.delayMs);
        System.sleep({D8}, FALLING, config.delayMs / 1000);
        readDHT();
        rtc.checkAlarms();
    } else {
        unsigned long currentMillis = millis();
        if (currentMillis - prevMillis >= config.delayMs) {
          readDHT();
          prevMillis = currentMillis;
        }
    }
    } else if (config.deepSleep) {
      Log.warn("Call setEnabled() to enable deep sleep");
      publish("warning", "Call setEnabled() to enable deep sleep");
    }
    // if (rtc.lostPower()) {
    //   Log.warn("RTC lost power and needs to be adjusted!");
    //   adjustRTC();
    // }
  }

  void readDHT() {
    float h = dht.getHumidity();
    float t = dht.getTempCelcius();
    float f = dht.getTempFarenheit();
    float k = dht.getTempKelvin();
    float dp = dht.getDewPoint();

    Log.info("Humid: %.2f%% - Temp: %.2f C / %.2f F / %.2f K - DewP: %.2f C", h,
             t, f, k, dp);

    publish("humidity", String::format("%.2f", h));
    publish("temperature", String::format("%.2f", f));
    publish("dew_point", String::format("%.2f", dp));
  }

  void firmwareCheck() {
    EEPROM.get(0, config);
    if (config.version == FIRMWARE_VERSION) {
      serializeConfig();
    } else {
      resetConfig();
    }
  }

  void resetConfig() {
    Log.warn("Firmware out of date; resetting config");
    config = defaultConfig;
    saveConfig();
  }

  size_t serializeConfig() {
    doc["delay"] = config.delayMs;
    doc["deepSleep"] = config.deepSleep;
    doc["enabled"] = config.enabled;
    return serializeJson(doc, configJson);
  }

  void saveConfig() {
    EEPROM.put(0, config);
    serializeConfig();
    publish("config", configJson);
  }

  int setDelay(String value) {
    unsigned int delayMs = value.toInt();
    if (delayMs == 0) {
      Log.warn("setDelay() called with non-integer or non-positive value: %s",
               value.c_str());
      publish(
          "error",
          String::format(
              "setDelay() called with non-integer or non-positive value: %s",
              value.c_str()));
      return 0;
    }
    if (delayMs != config.delayMs) {
      config.delayMs = delayMs;
      saveConfig();
      return 1;
    }
    return 0;
  }

  int enable(String ignored = "") {
    if (!config.enabled) {
      config.enabled = true;
      saveConfig();
      return 1;
    }
    return 0;
  }

  int disable(String ignored = "") {
    if (config.enabled) {
      config.enabled = false;
      saveConfig();
      return 1;
    }
    return 0;
  }

  int enableDeepSleep(String ignored = "") {
    if (!config.deepSleep) {
      config.deepSleep = true;
      saveConfig();
      return 1;
    }
    return 0;
  }

  int disableDeepSleep(String ignored = "") {
    if (config.deepSleep) {
      config.deepSleep = false;
      saveConfig();
      return 1;
    }
    return 0;
  }

  void publish(String topic, String data) {
    String fullTopic = String(PUBLISH_NAMESPACE) + "/" +
                       String(PUBLISH_DEVICE_NAME) + "/" + topic;
    Log.trace("PUBLISH <%s>: %s", fullTopic.c_str(), data.c_str());
    Particle.publish(fullTopic, data, PRIVATE);
  }