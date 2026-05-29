#include "burner_bridge.h"

#include "serial_log.h"
#include "ui_display.h"

namespace m5os::burner {

void logWorkflowToSerial() {
    log::info("burner_workflow",
              "M5Burner=base M5 OS flash once; apps stay on SD /apps/; "
              "re-flash M5 OS only after running an app or OS update");
}

String recoveryInstructions() {
    return "M5Burner/PlatformIO: flash M5 OS base only.\nApps remain on SD /apps/.";
}

void showHelpScreen() {
    logWorkflowToSerial();
    ui::drawBurnerHelp();
}

}  // namespace m5os::burner
