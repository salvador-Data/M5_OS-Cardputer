#pragma once

#include <Arduino.h>

namespace m5os::burner {

void logWorkflowToSerial();
void showHelpScreen();
String recoveryInstructions();

}  // namespace m5os::burner
