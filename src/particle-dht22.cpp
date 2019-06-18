#include "ArduinoJson.h"
#include "Adafruit_DHT.h"
#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);

#define DHT_PIN D2
#define DHT_TYPE 22
#define PUBLISH_NAMESPACE "worm"
#define PUBLISH_DEVICE_NAME "mindflayer"

#define FIRMWARE_VERSION 1

DHT dht(DHT_PIN, DHT_TYPE);

struct Config {
  uint8_t version;
  uint16_t delayMs;
  bool enabled;
  bool deepSleep;
};

size_t serializeConfig();
void saveConfig();
int enable(String ignored);
int disable(String ignored);
int enableDeepSleep(String ignored);
int disableDeepSleep(String ignored);
int setDelay(String ms);
void publish(String topic, String data);

Config config;
DynamicJsonDocument doc(JSON_OBJECT_SIZE(3));
char configJson[128];

void setup() {
  Serial.begin(152000);
  EEPROM.get(0, config);
  if (config.version != FIRMWARE_VERSION) {
    Config defaultConfig = {FIRMWARE_VERSION, 900000, true, true};
    config = defaultConfig;
    saveConfig();
  } else {
    serializeConfig();
  }

  Particle.function("enable", enable);
  Particle.function("disable", disable);
  Particle.function("enableDeepSleep", enableDeepSleep);
  Particle.function("disableDeepSleep", disableDeepSleep);
  Particle.function("setDelay", setDelay);
  Particle.variable("config", configJson);

  dht.begin();
}

void loop() {
  if (config.deepSleep) {
    // D1 is just a placeholder
    System.sleep({D1}, RISING, config.delayMs / 1000);
  } else {
    delay(config.delayMs);
  }

  if (config.enabled) {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a
    // very slow sensor)
    float h = dht.getHumidity();
    // Read temperature as Celsius
    float t = dht.getTempCelcius();
    // Read temperature as Farenheit
    float f = dht.getTempFarenheit();

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
      publish("error", "Could not read from DHT sensor");
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Compute heat index
    // Must send in temp in Fahrenheit!
    float hi = dht.getHeatIndex();
    float dp = dht.getDewPoint();
    float k = dht.getTempKelvin();

    publish("humidity", String::format("%.2f", h));
    publish("temperature", String::format("%.2f", f));
    publish("dew_point", String::format("%.2f", dp));
    publish("heat_index", String::format("%.2f", hi));

    Serial.print("Humid: ");
    Serial.print(h);
    Serial.print("% - ");
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.print("*C ");
    Serial.print(f);
    Serial.print("*F ");
    Serial.print(k);
    Serial.print("*K - ");
    Serial.print("DewP: ");
    Serial.print(dp);
    Serial.print("*C - ");
    Serial.print("HeatI: ");
    Serial.print(hi);
    Serial.println("*C");
    Serial.println(Time.timeStr());
  }
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
  uint8_t delayMs = value.toInt();
  if (delayMs > 0 && delayMs != config.delayMs) {
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
  Particle.publish(String(PUBLISH_NAMESPACE) + "/" +
                       String(PUBLISH_DEVICE_NAME) + "/" + topic,
                   data, PRIVATE);
}