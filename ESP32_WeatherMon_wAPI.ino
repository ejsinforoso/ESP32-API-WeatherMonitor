#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define DHTPIN 4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// WiFi credentials
const char* ssid = "BatoBalani";
const char* password = "Cr@igDavid971";

// Static IP configuration
IPAddress staticIP(192, 168, 0, 107);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 0, 1);
IPAddress dns(8, 8, 8, 8); // Google DNS

// API endpoint
const char* apiUrl = "https://esp32-api-2j8g.onrender.com/data";

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  WiFi.config(staticIP, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize OTA
  setupOTA();
}

void loop() {
  ArduinoOTA.handle(); // Handle OTA updates
  
  if (WiFi.status() == WL_CONNECTED) {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    
    float dewPoint = calculateDewPoint(temperature, humidity);
    float wetBulb = calculateWetBulb(temperature, humidity);
    
    // Create JSON payload
    DynamicJsonDocument doc(1024);
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["dew_point"] = dewPoint;
    doc["wet_bulb"] = wetBulb;
    
    String payload;
    serializeJson(doc, payload);
    
    // Send to API
    HTTPClient http;
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    WiFi.reconnect();
  }
  
  delay(60000); // Wait 1 minute between readings
}

void setupOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("ESP32-DHT22-Sensor");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA Update Service Ready");
  Serial.print("OTA Hostname: ");
  Serial.println(ArduinoOTA.getHostname());
}

float calculateDewPoint(float temp, float hum) {// Magnus formula
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