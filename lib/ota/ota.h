#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <ArduinoHTTPClient.h>

#include "ArduinoJson.h"
#include "SD.h"
#include "Update.h"
#include "utils.h"

String SIZE_DOWNLOADED = "downloadedChunkSize";
String DOWNLOAD_JSON = "download.json";
String BINARY = "binary";
String UPDATE_BINARY = "/binary.bin";

const char* headerKeys[] = {"checksum"};

struct Response {
    int size = 0;
    int statusCode = 0;
    uint32_t checksum;
    bool isValid = false;
};

class Ota {
   private:
    int chunkSize = 4000;
    bool isUpdateReady = false;

    StaticJsonDocument<1000> progressJson;

    bool updateProgress(int progressSize, String firmware) {
        if (readJsonFromSD(DOWNLOAD_JSON, progressJson)) {
            String binary = progressJson[BINARY];
            if (binary == firmware) {
                // Serial.println("Updating progress for " + binary);
                int previousProgress = progressSize == 0 ? 0 : progressJson[SIZE_DOWNLOADED];
                progressJson[SIZE_DOWNLOADED] = progressSize + previousProgress;
                return writeJsonToSD(DOWNLOAD_JSON, progressJson);
            } else {
                Serial.println("Previous progress was for " + binary);
                Serial.println("Creating new progress for " + firmware);
                progressJson[SIZE_DOWNLOADED] = 0;
                progressJson[BINARY] = firmware;
                return writeJsonToSD(DOWNLOAD_JSON, progressJson);
            }
        } else {
            Serial.println("Creating " + DOWNLOAD_JSON + " file!");
            Serial.println("Creating progress for " + firmware);
            progressJson[SIZE_DOWNLOADED] = progressSize;
            progressJson[BINARY] = firmware;
            return writeJsonToSD(DOWNLOAD_JSON, progressJson);
        }
    }

    Response httpRequest(Client& client, String path, int from = 0, int to = 0) {
        HttpClient* http;
        Response resp;
        std::string server = Split(path.c_str(), 0);
        http = new HttpClient(client, server.c_str());
        String request = path;
        if (to != 0) request = path + "?from=" + String(from) + "&to=" + String(to);
        int err;
        for (int i = 0; i < 5; ++i) {
            err = http->get(("http://" + request).c_str());
            if (err == 0) break;
            vTaskDelay(1);
        }
        if (err != 0) {
            resp.statusCode = err;
            return resp;
        }
        int httpCode = http->responseStatusCode();
        if (httpCode != 200) {
            resp.statusCode = httpCode;
            return resp;
        }

        while (http->headerAvailable()) {
            String name = http->readHeaderName();
            if (name == "checksum") {
                resp.checksum = strtoul(http->readHeaderValue().c_str(), NULL, 10);
                if (to != 0) break;
            }
            if (to == 0 && name == "filesize") {
                resp.size = atoi(http->readHeaderValue().c_str());
            }
        }
        if (to != 0) {
            int size = http->contentLength();
            uint8_t body[size];
            while ((http->connected() || http->available()) && (!http->endOfBodyReached())) {
                http->readBytes(body, size);
            }
            if (CRC32::calculate(body, size) == resp.checksum) {
                updateProgress(size, path);
                File binary = SD.open(UPDATE_BINARY, from == 0 ? FILE_WRITE : FILE_APPEND);
                for (int i = 0; i < size; i++) {
                    binary.write(body[i]);
                }
                binary.close();
                resp.isValid = true;
            }
        }
        return resp;
    }

    void removeAll(String firmware) {
        updateProgress(0, firmware);
        Serial.println("All downloaded progress is cleared!");  ///
    }

   public:
    bool getUpdate(String firmware, Client& client) {
        int count = 0;
        if (readJsonFromSD(DOWNLOAD_JSON, progressJson)) {
            String binary = progressJson[BINARY];
            if (binary == firmware) {
                count = progressJson[SIZE_DOWNLOADED];
            }
        }
        Response meta = httpRequest(client, firmware + "?meta=true");
        if (count != meta.size) {
            ///
            Serial.print("Starting from: ");
            Serial.println(count);
            Serial.print("Chunk size: ");
            Serial.println(chunkSize);
            Serial.print("File checksum: ");
            Serial.println(meta.checksum);
            Serial.print("|");
            for (int x = 0; x <= meta.size / chunkSize; x++) Serial.print(".");
            Serial.print("| ");
            Serial.println(meta.size);
            Serial.print("|");
            for (int x = 0; x < count / chunkSize; x++) Serial.print(".");
            ///

            bool noSuccess = false;
            for (;;) {
                Serial.print(".");  ///

                int to = count + chunkSize > meta.size ? meta.size : count + chunkSize;
                Response chunk = httpRequest(client, firmware, count, to);
                if (chunk.statusCode != 0) {
                    Serial.print("HTTP Error: ");
                    Serial.println(chunk.statusCode);
                    return false;
                }
                if (chunk.isValid) {
                    count += chunkSize;
                    if (count >= meta.size) {
                        Serial.println("| Finished");  ///
                        break;
                    }
                } else {
                    noSuccess = true;
                    for (int i = 0; i < 5; i++) {
                        Serial.print("x");  ///

                        Response reChunk = httpRequest(client, firmware, count, to);
                        if (reChunk.isValid) {
                            count += chunkSize;
                            if (count >= meta.size) {
                                Serial.println("| Finished");  ///
                                break;
                            }
                            noSuccess = false;
                            break;
                        }
                    }
                    if (noSuccess) {
                        Serial.println("| No success");  ///
                        removeAll(firmware);
                        return false;
                    }
                }
            }
        }

        Serial.println("Start updating firmware...");  ///
        if (updateFirmwareFromSD(UPDATE_BINARY)) {
            removeAll(firmware);
            Serial.println("Rebooting...");  ///
            delay(2000);
            ESP.restart();
        };
        removeAll(firmware);
        return true;
    }

    void CheckUnfinished() {
    }
};

#endif
