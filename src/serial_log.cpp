#include "serial_log.h"

#include <ArduinoJson.h>

namespace m5os::log {

void begin() {
    Serial.begin(115200);
    delay(120);
    JsonDocument doc;
    doc["app"] = "M5_OS-Cardputer";
    doc["event"] = "boot";
    doc["authorized_lab_only"] = true;
    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

void info(const char* event, const String& detail) {
    JsonDocument doc;
    doc["app"] = "M5_OS-Cardputer";
    doc["event"] = event;
    if (detail.length()) doc["detail"] = detail;
    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

void json(const char* event, const String& jsonBody) {
    JsonDocument doc;
    doc["app"] = "M5_OS-Cardputer";
    doc["event"] = event;
    JsonDocument body;
    if (!jsonBody.isEmpty() && !deserializeJson(body, jsonBody)) {
        doc["payload"] = body;
    }
    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

}  // namespace m5os::log
