#include "burner_bridge.h"

#include "serial_log.h"
#include "ui_display.h"

namespace m5os::burner {

void logWorkflowToSerial() {
    log::info("burner_workflow",
              "USB+M5Burner+Cardputer target+flash M5_OS bin; apps in SD /firmware/");
}

String recoveryInstructions() {
    return "Flash M5 OS via M5Burner or PlatformIO to return to launcher.";
}

void showHelpScreen() {
    logWorkflowToSerial();
    ui::drawBurnerHelp();
}

}  // namespace m5os::burner
