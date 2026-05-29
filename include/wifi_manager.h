#pragma once

#include <Arduino.h>

namespace m5os {

bool wifiConnectInteractive(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen);
/** Connect using credentials from settings.json (boot / after SD load). */
bool wifiTrySavedConnect();
bool wifiIsConnected();
String wifiIpAddress();

}  // namespace m5os
