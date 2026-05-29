#include "burner_bridge.h"

#include "serial_log.h"
#include "ui_display.h"

namespace m5os::burner {

void logWorkflowToSerial() {
    log::info("burner_workflow",
              "M5Burner desktop=base OS USB flash; on-device Flash from M5Burner catalog=LauncherHub OTA; "
              "apps also saved to SD /apps/; re-flash M5 OS base after running an app");
}

String recoveryInstructions() {
    return "USB: M5Burner/PlatformIO flash M5 OS base.\n"
           "On-device: Flash from M5Burner catalog.\n"
           "Apps stay on SD /apps/.";
}

void showHelpScreen() {
    logWorkflowToSerial();
    ui::drawBurnerHelp();
}

}  // namespace m5os::burner
