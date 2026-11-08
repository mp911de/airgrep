#pragma once

#include <cstdint>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// Sets the system clock from the ADSBee's PPS-disciplined time with AT+TIME_PING bursts.
namespace adsbee::timesync {
	struct State {
		bool set = false; // the system clock was set from the ADSBee at least once
		bool running = false; // a ping burst is scheduled or in progress
		int64_t synced_us = 0; // monotonic time of the last applied estimate
		int64_t correction_us = 0; // what the last estimate changed the clock by
		int64_t rtt_us = 0;
		uint32_t pps = 0;
		uint8_t answered = 0; // replies in the last burst
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

	// Called by the link: subscribes to TIME_PING replies and starts the periodic check.
	esp_err_t start();
	// Schedules a ping burst. ESP_ERR_INVALID_STATE while one is due or the link is down.
	esp_err_t schedule_nss_time_sync();
}
