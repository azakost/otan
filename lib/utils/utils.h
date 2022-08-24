#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <CRC32.h>

#include "ArduinoJson.h"
#include "SD.h"
extern "C" {
#include "crypto/base64.h"
}

bool readJsonFromSD(String filename, JsonDocument &doc) {
    File file = SD.open("/" + filename, FILE_READ);
    if (file) {
        DeserializationError error = deserializeJson(doc, file);
        if (error) {
            Serial.println("Failed to read " + filename);
            return false;
        }
        file.close();
        return true;
    }
    return false;
}

bool writeJsonToSD(String filename, StaticJsonDocument<1000> doc) {
    File file = SD.open("/" + filename, FILE_WRITE);
    if (file) {
        size_t result = serializeJson(doc, file);
        if (result <= 0) {
            Serial.println("Nothing was written! Result size: " + result);
            return false;
        }
        file.close();
        return true;
    }
    Serial.println("Failed to write to " + filename);
    return false;
}

char *StdToChar(std::string str) {
    char *cstr = new char[str.length() + 1];
    strcpy(cstr, str.c_str());
    return cstr;
}

char *decodeBase64(String body, size_t &outputLength) {
    char *toDecode = StdToChar(body.c_str());
    unsigned char *decoded = base64_decode((const unsigned char *)toDecode, strlen(toDecode), &outputLength);
    return (char *)decoded;
}

std::string Split(const char *data, int index) {
    char separator = '/';
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = String(data).length() - 1;
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (String(data).charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? String(data).substring(strIndex[0], strIndex[1]).c_str() : "";
}

bool updateFirmwareFromSD(String path) {
    File updateBin = SD.open(path, FILE_READ);
    size_t updateSize = updateBin.size();
    if (Update.begin(updateSize)) {
        size_t written = Update.writeStream(updateBin);
        if (written == updateSize) {
            Serial.println("Written : " + String(written) + " successfully");
        } else {
            Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
        }
        if (Update.end()) {
            Serial.println("OTA done!");
            if (Update.isFinished()) {
                Serial.println("Update successfully completed.");
                return true;
            } else {
                Serial.println("Update not finished? Something went wrong!");
                return false;
            }
        } else {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            return false;
        }
    } else {
        Serial.println("Not enough space to begin OTA");
        return false;
    }
}

#endif