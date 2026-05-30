#pragma once

#include "m5os_config.h"

#include <soc/rtc_cntl_reg.h>

/** NVS + paths shared between M5 OS (app0) and session gateway (app1). */

namespace m5os::gateway {



constexpr char kNvsNamespace[] = "m5os";

constexpr char kGatewayActiveKey[] = "gw_active";

constexpr char kStagingPath[] = "/home/default/.staging/run_app.bin";



constexpr unsigned kMinGatewayUiMs = 2000;

constexpr unsigned kAutoLaunchMs = 6000;

constexpr unsigned kEscHoldMs = 1000;

/** Tell custom bootloader to honor otadata on the next SW reset (Load app / launch run slot). */
inline void setStagedBootHandoff() { REG_WRITE(RTC_CNTL_STORE0_REG, kRtcBootStagedMagic); }

}  // namespace m5os::gateway

