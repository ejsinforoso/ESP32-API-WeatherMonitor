#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#define DHTPIN 23
#define DHTTYPE DHT22

// Configuration
const char* ssid = "BatoBalani";
const char* password = "Cr@igDavid971";
const char* apiUrl = "https://esp32-api-2j8g.onrender.com/data";
const char* otaHostname = "ESP32-WeatherMon";

// Network configuration
IPAddress staticIP(192, 168, 0, 107);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

DHT dht(DHTPIN, DHTTYPE);
unsigned long lastReadingTime = 0;
const unsigned long readingInterval = 15000; // 15 seconds between readings
bool wifiConnected = false;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait only if USB is connected
  
  Serial.println("\nStarting ESP32 Weather Monitor");
  Serial.println("============================");
  
  dht.begin();
  initWiFi();
  initOTA();
  
  lastReadingTime = millis() - readingInterval; // Trigger immediate first reading
}

void loop() {
  ArduinoOTA.handle();
  
  // WiFi management
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("WiFi disconnected!");
      wifiConnected = false;
    }
    reconnectWiFi();
  } else if (!wifiConnected) {
    wifiConnected = true;
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  // Take and send readings
  if (millis() - lastReadingTime >= readingInterval) {
    takeReading();
    lastReadingTime = millis();
  }
}

void initWiFi() {
  WiFi.config(staticIP, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
}

void reconnectWiFi() {
  static unsigned long lastAttempt = 0;
  const unsigned long retryInterval = 10000; // 10 seconds between retries
  
  if (millis() - lastAttempt >= retryInterval) {
    Serial.println("Attempting to reconnect WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    lastAttempt = millis();
  }
}

void initOTA() {
  ArduinoOTA.setHostname(otaHostname);
  
  ArduinoOTA
    .onStart([]() {
      String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
      Serial.println("OTA Update Started: " + type);
    })
    .onEnd([]() {
      Serial.println("\nOTA Update Complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA Update Service Ready");
  Serial.print("OTA Hostname: ");
  Serial.println(otaHostname);
}

void takeReading() {
  Serial.println("\nTaking sensor reading...");
  
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  float dewPoint = calculateDewPoint(temperature, humidity);
  float wetBulb = calculateWetBulb(temperature, humidity);
  
  // Create and print JSON payload
  DynamicJsonDocument doc(256);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["dew_point"] = dewPoint;
  doc["wet_bulb"] = wetBulb;
  
  String payload;
  serializeJson(doc, payload);
  
  Serial.println("Payload to send:");
  Serial.println(payload);
  
  // Send to API if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    sendDataToAPI(payload);
  } else {
    Serial.println("Cannot send - WiFi not connected");
  }
}

void sendDataToAPI(const String& payload) {
  Serial.println("Sending data to API...");
  
  HTTPClient http;
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    
    String response = http.getString();
    if (response.length() > 0) {
      Serial.print("Server response: ");
      Serial.println(response);
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

float calculateDewPoint(float temp, float hum) {
  float a = 17.27;
  float b = 237.7;
  float alpha = ((a * temp) / (b + temp)) + log(hum/100.0);
  return (b * alpha) / (a - alpha);
}

float calculateWetBulb(float temp, float hum) {
  return temp * atan(0.151977 * sqrt(hum + 8.313659)) + 
         atan(temp + hum) - atan(hum - 1.676331) + 
         0.00391838 * pow(hum, 1.5) * atan(0.023101 * hum) - 4.686035;
}