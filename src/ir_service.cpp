#include "ir_service.h"

#include <IRremote.hpp>

static bool gIrReady = false;

void irInit() {
    IrSender.begin(IR_TX_PIN);
    IrReceiver.begin(IR_RX_PIN, ENABLE_LED_FEEDBACK);
    gIrReady = true;
}

bool irSendNec(uint16_t address, uint16_t command, uint8_t repeats) {
    if (!gIrReady) irInit();
    IrSender.sendNEC(address, command, repeats);
    return true;
}
