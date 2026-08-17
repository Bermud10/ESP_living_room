#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// --- НАСТРОЙКИ WI-FI ---
const char* ssid = "TP-Link_B400";
const char* password = "33208381";

// --- НАСТРОЙКИ MQTT (Clusterfly) ---
const char* mqtt_server = "srv2.clusterfly.ru";
const int mqtt_port = 9991; // Обычный MQTT порт
const char* mqtt_user = "user_1d18b030";
const char* mqtt_password = "1bz78-sYP3T8u"; // Ваш пароль

// Топики ОБЯЗАТЕЛЬНО должны начинаться с вашего user_id
const char* topic_temp = "user_1d18b030/room/temp";
const char* topic_humidity = "user_1d18b030/room/humidity";
const char* topic_pressure = "user_1d18b030/room/pressure";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme;

unsigned long lastMsg = 0;
char msg[50];

// ФУНКЦИЯ ПЕРЕПОДКЛЮЧЕНИЯ К MQTT 
void reconnect() {
  byte tries = 10;
  while (--tries && !client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP8266_Room_";
    clientId += String(random(0xffff), HEX);
    
    // ⭐ Обязательно передаем mqtt_user и mqtt_password!
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
  
  if (!client.connected()) {
    Serial.println("❌ Не удалось подключиться к MQTT. Ждем 30 секунд...");
    delay(30000);
  }
}
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== Комнатный датчик инициализируется ===");
  
  // ⭐ Явно инициализируем шину I2C ОДИН раз перед циклом
  Wire.begin(5, 4); // SDA=D1(GPIO5), SCL=D2(GPIO4)

  // Инициализация BME280 с попытками
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
    Serial.println("🛑 Датчик BME280 не найден!");
    Serial.println("Программа остановлена. Проверьте провода и питание (3.3V).");
    while (1) delay(10); // Останавливаем программу навсегда
  }
  
  // Подключение к Wi-Fi
  byte trys = 10;
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (--trys && WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("\nОшибка подключения к WiFi, код: ");
    Serial.println(WiFi.status());
    Serial.println("Программа остановлена. Проверьте SSID и пароль.");
    while (1) delay(10);
  }

  Serial.println("\n✅ WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // ⭐ НАСТРОЙКА MQTT: ИСПРАВЛЯЕМ ПОРТ НА 9991!
  client.setServer(mqtt_server, 9991); 
  
  // Увеличиваем таймаут сокета для надежности (ESP8266 иногда тормозит)
  client.setSocketTimeout(10); 
}

// --- LOOP ---
void loop() {
  // Поддержание соединения с MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // ⭐ ВАЖНО: Отправляем данные ТОЛЬКО если есть подключение!
  if (client.connected()) {
    unsigned long now = millis();
    if (now - lastMsg > 90000) {
      lastMsg = now;
      
      float temperature = bme.readTemperature();
      float humidity = bme.readHumidity();
      
      if (isnan(temperature) || isnan(humidity)) {
        Serial.println("❌ Ошибка чтения с датчика BME280");
        return;
      }
      
      dtostrf(temperature, 1, 2, msg);
      client.publish(topic_temp, msg, true);
      Serial.print("📤 Температура: ");
      Serial.print(msg);
      Serial.println(" °C");
      
      dtostrf(humidity, 1, 2, msg);
      client.publish(topic_humidity, msg, true);
      Serial.print("📤 Влажность: ");
      Serial.print(msg);
      Serial.println(" %");
      
      Serial.println("---");
    }
  }
}