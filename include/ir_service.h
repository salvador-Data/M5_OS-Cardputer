#pragma once

#include <stdint.h>

#ifndef IR_TX_PIN
#define IR_TX_PIN 44
#endif
#ifndef IR_RX_PIN
#define IR_RX_PIN 1
#endif

void irInit();
bool irSendNec(uint16_t address, uint16_t command, uint8_t repeats = 2);
