#include <Arduino.h>
#include <WiFi.h>

#include "SD.h"
#include "ota.h"

Ota ota = Ota();

const char* ssid = "WISHBOX";
const char* password = "87776420344";

void setup() {
    Serial.begin(115200);
    Serial.print("Initializing SD card...");
    if (!SD.begin(22)) {
        Serial.println("Couldn't open SD card");
        delay(2000);
        return;
    }

    Serial.println("Initializing Wi-Fi...");
    WiFi.begin(ssid, password);
    Serial.println("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());

     if (WiFi.status() == WL_CONNECTED) {
        WiFiClient wifiClient = WiFiClient();
        ota.getUpdate("upgrades.smartvend.kz/empty_firmware.bin", wifiClient);
    }
}

void loop() {
    // put your main code here, to run repeatedly:
}