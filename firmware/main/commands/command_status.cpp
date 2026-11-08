#include "command_status.hpp"
#include <cstdio>
#include "esp_timer.h"

namespace command_status {
	bool header(Status status, int64_t updated_us, esp_err_t last_error, int64_t error_us) {
		int64_t now = esp_timer_get_time();
		if (!updated_us) std::printf("%-20s %s\n", "Status", status == Status::off ? "OFF" : "UNKNOWN");
		else std::printf("%-20s %lld ms ago\n", "Updated", (long long)((now - updated_us) / 1000));
		if (last_error != ESP_OK)
			std::printf("%-20s %s, %lld ms ago\n", "Last error", esp_err_to_name(last_error),
				(long long)((now - error_us) / 1000));
		return updated_us != 0;
	}

	int ack(esp_err_t result) {
		if (result == ESP_OK) std::puts("OK");
		else std::printf("FAIL %s\n", result == ESP_ERR_TIMEOUT ? "timeout" : esp_err_to_name(result));
		return result == ESP_OK ? 0 : 1;
	}
}
