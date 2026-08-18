#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "TP-Link_B400";
const char* password = "33208381";

const char* mqtt_server = "srv2.clusterfly.ru";
const int mqtt_port = 9991;
const char* mqtt_user = "user_1d18b030";
const char* mqtt_password = "1bz78-sYP3T8u";

const char* topic_temp = "user_1d18b030/room/temp";
const char* topic_humidity = "user_1d18b030/room/humidity";
const char* topic_pressure = "user_1d18b030/room/pressure";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme;

//NTP клиент для синхронизации времени
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 18000, 60000); 
// 10800 = UTC+3 (московское время), 60000 = обновление раз в минуту

unsigned long lastMsg = 0;
char msg[100]; 

void reconnect() {
  byte tries = 10;
  while (--tries && !client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266_Room_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Комнатный датчик (BME280) ===");
  
  Wire.begin(5, 4);
  
  int maxRetries = 5;
  bool sensorFound = false;
  for (int i = 1; i <= maxRetries; i++) {
    Serial.print("Попытка инициализации BME280 (");
    Serial.print(i);
    Serial.println(" из 5)...");
    if (bme.begin(0x76)) {
      sensorFound = true;
      Serial.println("✅ Датчик найден!");
      break;
    }
    Serial.println("❌ Не удалось найти датчик.");
    delay(1000);
  }
  if (!sensorFound) {
    Serial.println("🛑 Датчик не найден! Программа остановлена.");
    while (1) delay(10);
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  byte trys = 10;
  while (--trys && WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n🛑 Ошибка WiFi. Программа остановлена.");
    while (1) delay(10);
  }
  Serial.println("\n✅ WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  //Синхронизация времени через NTP
  Serial.println(" Синхронизация времени через NTP...");
  timeClient.begin();
  int ntpRetries = 0;
  while (!timeClient.update() && ntpRetries < 10) {
    timeClient.forceUpdate();
    delay(500);
    ntpRetries++;
  }
  
  if (timeClient.isTimeSet()) {
    Serial.print("✅ Время синхронизировано: ");
    Serial.println(timeClient.getFormattedTime());
  } else {
    Serial.println("️ Не удалось синхронизировать время, используем uptime");
  }
  
  client.setServer(mqtt_server, mqtt_port);
  client.setSocketTimeout(10);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Обновляем время каждый цикл
  timeClient.update();
  
  if (client.connected()) {
    unsigned long now = millis();
    if (now - lastMsg > 30000) {
      lastMsg = now;
      
      float temperature = bme.readTemperature();
      float humidity = bme.readHumidity();
      float pressure = bme.readPressure() / 100.0F;
      
      if (isnan(temperature) || isnan(humidity)) {
        Serial.println("❌ Ошибка чтения датчика");
        return;
      }
      
      //Получаем Unix timestamp (секунды с 01.01.1970)
      unsigned long timestamp = timeClient.getEpochTime();
      
      //  Формируем JSON с данными и временем
      snprintf(msg, sizeof(msg), 
        "{\"temp\":%.2f,\"hum\":%.2f,\"press\":%.1f,\"ts\":%lu}",
        temperature, humidity, pressure, timestamp
      );
      
      // Отправляем JSON в один топик (или можно разбить на несколько)
      client.publish("user_1d18b030/room/data", msg, true);
      
      Serial.print("📤 Отправлено: ");
      Serial.println(msg);
      Serial.print("   Время измерения: ");
      Serial.println(timeClient.getFormattedTime());
      Serial.println("---");
    }
  }
}