#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <math.h>
#include "Adafruit_INA219_changed.h"

Adafruit_INA219 ina219;

const char* ssid = "CyanoCaptureWiFi";
const char* password = "Bloomgashydro2#";
String serverName = "http://192.168.0.125:3484/current/";  // Server API endpoint

class Sensors {
  public:
    Sensors(String name, float val) {
      this->S_id = name;
      this->S_val = val;
    };
    String S_id;
    float S_val = -1;
};

Sensors shuntvoltage("shuntvoltage", 0.0);
Sensors busvoltage("busvoltage", 0.0);
Sensors loadvoltage("loadvoltage", 0.0);
Sensors current_uA("current_uA", 0.0);
Sensors power_mW("power_mW", 0.0);

String getWifiStatus(int status) {
  switch (status) {
        case WL_IDLE_STATUS: return "IDLE_STATUS";
        case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
        case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_CONNECTED: return "CONNECTED";
        case WL_DISCONNECTED: return "DISCONNECTED";
  }
}

String floatToStringRounded(float value, int decimal_places) {
    char buffer[20];  // Buffer to store the string representation
    sprintf(buffer, "%.*f", decimal_places, value);  // Convert float to string with specified decimal places
    return String(buffer);  // Return the result as an Arduino String
}

void connectToWiFi() {
  int status = WL_IDLE_STATUS;
    WiFi.begin(ssid, password);

    while (status != WL_CONNECTED) {
        delay(1000);
        status = WiFi.status();
        Serial.println(getWifiStatus(status));
    }
}

// Calibration function
float calibrateCurrent(float original_current) {
    float m = 0.17286016439072913;  // Slope
    float c = 0.3539092126263919;  // Y-intercept

    float value = m * original_current + c;
    value *= 10;  // Multiply by 10 to shift decimal place
    value = round(value);  // Round to nearest whole number
    value /= 10;  // Divide by 10 to shift decimal back
    return value;
}

void setup() {
  Wire.begin();
  Serial.begin(115200);

  connectToWiFi();

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
  }
}

void loop() {
  if(WiFi.status() == WL_CONNECTED) {
    shuntvoltage.S_val = ina219.getShuntVoltage_mV();
    busvoltage.S_val = ina219.getBusVoltage_V();
    loadvoltage.S_val = busvoltage.S_val + (shuntvoltage.S_val / 1000);

    // Get the raw current and calibrate it
    float raw_current = ina219.getCurrent_mA();
    current_uA.S_val = calibrateCurrent(raw_current);
    String calibrated_current_str = floatToStringRounded(current_uA.S_val, 1);

    power_mW.S_val = ina219.getPower_mW();

    HTTPClient http;
    String serverPath = serverName;

    http.begin(serverPath.c_str());
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> jsonDoc;
    jsonDoc["shuntvoltage"] = shuntvoltage.S_val;
    jsonDoc["busvoltage"] = busvoltage.S_val;
    jsonDoc["loadvoltage"] = loadvoltage.S_val;
    jsonDoc["current_uA"] = calibrated_current_str;
    jsonDoc["power_mW"] = power_mW.S_val;

    char jsonBuffer[256];
    serializeJson(jsonDoc, jsonBuffer);

    int httpResponseCode = http.POST(jsonBuffer);

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }

  delay(1000);
}
