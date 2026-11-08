#pragma once

#include <cstddef>
#include <cstdint>

namespace adsbee {
#define ADSBEE_RX_TIMEOUT_MS 3000
#define ADSBEE_LIVENESS_STALE_MS 500

	bool valid_csbee(const char* text, size_t length);
	// A command fits the buffer with its terminator and carries no line break itself.
	bool acceptable_command(const char* command, size_t capacity);

	struct Reply {
		char text[2048]{};
		uint16_t length = 0;
		bool append(const char* line, size_t size);
	};

	struct Power {
		bool on = false;
		int64_t updated_us = 0, error_us = 0;
		bool permits_tx(int64_t now_us) const;
	};

	class Link {
	public:
		static constexpr int64_t kRxTimeoutUs = ADSBEE_RX_TIMEOUT_MS * 1000;
		static constexpr int64_t kStaleTimeoutUs = ADSBEE_LIVENESS_STALE_MS * 1000;
		void update_power(Power power, int64_t now_us);
		void received(int64_t received_us);
		void poll(int64_t now_us);
		bool permits_tx(int64_t now_us) const;

	private:
		Power power_;
		int64_t powered_since_us_ = 0, received_us_ = 0;
	};
}
