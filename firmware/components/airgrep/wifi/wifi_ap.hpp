#pragma once

#include <cstdint>

#include "device.h"
#include "esp_err.h"
#include "locked_state.hpp"
#include "wifi_protocol.hpp"

// The ADSBee's ESP32 runs the access point, we only switch it. The Wi-Fi
// slider is the source of truth: every flip sends AT+WIFI_AP, nothing is ever
// saved on the ADSBee. Every WIFI_AP_POLL_PERIOD_MS the ADSBee is asked what
// it runs and reconfigured when that differs, which also heals an ADSBee
// reboot back to its saved settings.
namespace wifi {
#define WIFI_AP_PASSWORD "airgrep-wifi"
#define WIFI_AP_CHANNEL 11
#define WIFI_AP_POLL_PERIOD_MS 60000

struct State : StateData, LockedState<State> {};

// Subscribes to wifi_switch_changed.
// ssid is the Device ID.
esp_err_t prepare(const char* ssid);
esp_err_t start();
// Queues a resend of the current slider position. ESP_ERR_INVALID_STATE
// before the first slider read or while the link is down.
esp_err_t apply_wifi_state();
esp_err_t query_access_point();
}
