
#include "ArduinoJson.h"
#include "Adafruit_DHT.h"
#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);

#define DHT_PIN D2
#define DHT_TYPE 22
#define PUBLISH_NAMESPACE "worm"
#define PUBLISH_DEVICE_NAME "mindflayer"
#define DEFAULT_DELAY 900000
#define DEFAULT_ENABLE true
#define DEFAULT_DEEP_SLEEP false
#define FIRMWARE_VERSION 5
#define FIRMWARE_NAME "particle-dht22"
#define STARTUP_DELAY 3000

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

  firmwareCheck();

  Particle.function("enable", enable);
  Particle.function("disable", disable);
  Particle.function("enableDeepSleep", enableDeepSleep);
  Particle.function("disableDeepSleep", disableDeepSleep);
  Particle.function("setDelay", setDelay);
  Particle.variable("config", configJson);

  Log.info("Config: %s", configJson);
  dht.begin();
  Log.trace("Performing initial read");
  delay(STARTUP_DELAY);

  readDHT();
  prevMillis = millis();
}

void loop() {
  if (config.enabled) {
    if (config.deepSleep) {
      Log.info("Sleeping for %dms", config.delayMs);
      System.sleep({D1}, RISING, config.delayMs / 1000);
      readDHT();
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
}

void readDHT() {
  float h = dht.getHumidity();
  float t = dht.getTempCelcius();
  float f = dht.getTempFarenheit();
  float k = dht.getTempKelvin();
  float dp = dht.getDewPoint();

  Log.info("Humid: %.2f%% - Temp: %.2f*C / %.2f*F / %.2f*K - DewP: %.2f*C", h,
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
    publish("error",
            String::format(
                "setDelay() called with non-integer or non-positive value: %s",
                value.c_str()));
    return 0;
  }
  if (delayMs != config.delayMs) {
    config.delayMs = delayMs;
    return 1;
  }
  return 0;
}

int enable(String ignored = "") {
  if (!config.enabled) {
    config.enabled = true;
    return 1;
  }
  return 0;
}

int disable(String ignored = "") {
  if (config.enabled) {
    config.enabled = false;
    return 1;
  }
  return 0;
}

int enableDeepSleep(String ignored = "") {
  if (!config.deepSleep) {
    config.deepSleep = true;
    return 1;
  }
  return 0;
}

int disableDeepSleep(String ignored = "") {
  if (config.deepSleep) {
    config.deepSleep = false;
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