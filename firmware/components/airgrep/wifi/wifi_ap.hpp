#pragma once

#include <cstdint>
#include "device.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "wifi_protocol.hpp"

// The ADSBee's ESP32 runs the access point, we only switch it. The Wi-Fi slider is the
// source of truth: every flip sends AT+WIFI_AP, nothing is ever saved on the ADSBee. Every
// WIFI_AP_POLL_PERIOD_MS the ADSBee is asked what it runs and reconfigured when that differs,
// which also heals an ADSBee reboot back to its saved settings.
namespace wifi {
#define WIFI_AP_PASSWORD "airgrep-wifi"
#define WIFI_AP_CHANNEL 11
#define WIFI_AP_POLL_PERIOD_MS 60000
// After TX attach the time schedule_nss_time_sync burst owns the link for about a second.
#define WIFI_AP_ATTACH_DELAY_MS 2000
// Query after a set, once its reply window has closed.
#define WIFI_AP_VERIFY_DELAY_MS 1000

	struct State {
		Status status = Status::unknown;
		bool switch_known = false; // the slider was read at least once
		bool wanted = false; // slider position, Wi-Fi on
		bool reported_known = false;
		bool pending_apply = false; // a slider position the link could not take yet
		Config reported; // last +WIFI_AP= reply
		int64_t sent_us = 0, reported_us = 0;
		uint32_t sent = 0, mismatches = 0;
		esp_err_t last_error = ESP_OK;
		int64_t error_us = 0;

		template <typename F>
		static void update(F f) {
			portENTER_CRITICAL(&mutex_);
			f(storage());
			portEXIT_CRITICAL(&mutex_);
		}

		static State copy();

	private:
		static State& storage();
		static portMUX_TYPE mutex_;
	};

	// ssid is the Device ID. Subscribes to the expander, so call before expander init.
	esp_err_t start(const char* ssid);
	// Sends the slider position to the ADSBee now. ESP_ERR_INVALID_STATE before the first
	// slider read or while the link is down.
	esp_err_t apply();
	// Sends the query the periodic poll sends.
	esp_err_t poll();
	// Link hook after the init sequence, runs on the loop.
	void attached();
}
