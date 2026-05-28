#pragma once

#include <Arduino.h>

namespace m5os {

bool wifiConnectInteractive(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen);
bool wifiIsConnected();
String wifiIpAddress();

}  // namespace m5os
