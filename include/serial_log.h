#pragma once

#include <Arduino.h>

namespace m5os::log {

void begin();
void info(const char* event, const String& detail = "");
void json(const char* event, const String& jsonBody);

}  // namespace m5os::log
