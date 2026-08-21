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

const char* topic_data = "user_1d18b030/room/data";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ru.pool.ntp.org", 0, 60000); 

unsigned long lastMsg = 0;
char msg[150];

// Функция безопасного ожидания (кормит сторожевой таймер)
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    delay(10);
    yield();  //Предотвращает перезагрузку WDT
  }
}

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
      safeDelay(5000); // Используем безопасную задержку
    }
  }
  
  if (!client.connected()) {
    Serial.println("❌ Не удалось подключиться к MQTT. Ждем 10 секунд...");
    safeDelay(10000);
  }
}

void setup() {
  Serial.begin(115200);
  safeDelay(1000); // Безопасная задержка при старте
  
  Serial.println("\n=== Комнатный датчик (BME280) ===");
  
  Wire.begin(5, 4); // SDA=D1, SCL=D2
  
  // 1. Инициализация датчика
  if (!bme.begin(0x76)) {
    Serial.println("Датчик BME280 не найден!");
  } else {
    Serial.println("Датчик BME280 найден!");
  }
  
  // 2. Подключение к Wi-Fi с защитой от зависания
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    Serial.print(".");
    safeDelay(500);
    wifiAttempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n🛑 Ошибка WiFi. Перезагрузка через 5 секунд...");
    safeDelay(5000);
    ESP.restart();
  }
  
  Serial.println("\n✅ WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // 3. Синхронизация времени (НЕБЛОКИРУЮЩАЯ)
  Serial.println("Синхронизация времени через NTP...");
  timeClient.begin();
  bool timeSynced = false;
  for (int i = 1; i <= 5; i++) {
    if (timeClient.update()) {
      timeSynced = true;
      break;
    }
    safeDelay(500);
  }
  
  if (timeSynced) {
    Serial.print("✅ Время синхронизировано: ");
    Serial.println(timeClient.getFormattedTime());
  } else {
    Serial.println("Не удалось синхронизировать время. Продолжаем работу без NTP.");
  }
  
  // 4. Настройка MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setSocketTimeout(10); // Увеличиваем таймаут для медленных сетей
}

void loop() {
  // Поддержание соединения MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Обновляем время в фоне (библиотека сама решит, нужно ли делать запрос)
  timeClient.update();
  
  if (client.connected()) {
    unsigned long now = millis();
    if (now - lastMsg > 120000) { // Отправка каждые 2 минуты
      lastMsg = now;
      
      float temperature = bme.readTemperature();
      float humidity = bme.readHumidity();
      float pressure = bme.readPressure() / 100.0F;
      
      if (isnan(temperature) || isnan(humidity)) {
        Serial.println("❌ Ошибка чтения датчика BME280");
        return;
      }
      
      unsigned long timestamp = timeClient.getEpochTime();
      
      snprintf(msg, sizeof(msg), 
        "{\"temp\":%.2f,\"hum\":%.2f,\"press\":%.1f,\"ts\":%lu}",
        temperature, humidity, pressure, timestamp
      );
      
      if (client.publish(topic_data, msg, true)) {
        Serial.print("📤 Отправлено: ");
        Serial.println(msg);
      } else {
        Serial.println("❌ Ошибка публикации MQTT!");
      }
      Serial.println("---");
    }
  }
}