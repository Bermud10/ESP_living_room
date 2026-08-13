#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

const char* ssid = "TP-Link_B400";
const char* password = "33208381";

const char* mqtt_server = "broker.hivemq.com";
const char* topic_temp = "bermud10_test/room/temp";
const char* topic_humidity = "bermud10_test/room/humidity";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme;

unsigned long lastMsg = 0;
char msg[50];

// ФУНКЦИЯ ПЕРЕПОДКЛЮЧЕНИЯ К MQTT 
void reconnect() {
  byte tries = 10;
  while (--tries && !client.connected() ) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP8266_Room_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
  delay(60000);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== Комнатный датчик инициализируется ===");
  
  // Инициализация BME280 с попытками
  int maxRetries = 5; 
  bool sensorFound = false; 

  for (int i = 1; i <= maxRetries; i++) {
    Serial.print("Попытка инициализации BME280 (");
    Serial.print(i);

    if (bme.begin(0x76)) {
      sensorFound = true;
      Serial.println("Датчик найден!");
      break; 
    }

    Serial.println("Не удалось найти датчик.");
    delay(1000); 
  }

  if (!sensorFound) {
    Serial.println("Датчик BME280 не найден!");
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

  if(WiFi.status() != WL_CONNECTED){
   Serial.print("Ошибка подключения к WiFi, - ");
   Serial.println(WiFi.status());
   Serial.println("Программа остановлена. Проверьте SSID и пароль.");
   while (1) delay(10);
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Настройка MQTT
  client.setServer(mqtt_server, 1883);
}

// --- LOOP ---
void loop() {
  // Поддержание соединения с MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Публикация данных каждые 10 секунд
  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    
    // Чтение данных с BME280
    float temperature = bme.readTemperature();
    float humidity = bme.readHumidity();
    
    // Проверка на ошибки чтения
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Ошибка чтения с датчика BME280");
      return;
    }
    
    // Форматирование и публикация температуры
    dtostrf(temperature, 1, 2, msg);
    client.publish(topic_temp, msg, true); // true = retain (запомнить последнее значение)
    Serial.print("Published temperature: ");
    Serial.print(msg);
    Serial.println(" °C");
    
    // Форматирование и публикация влажности
    dtostrf(humidity, 1, 2, msg);
    client.publish(topic_humidity, msg, true); // true = retain
    Serial.print("Published humidity: ");
    Serial.print(msg);
    Serial.println(" %");
    
    Serial.println("---");
  }
}