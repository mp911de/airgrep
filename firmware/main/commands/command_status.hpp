#pragma once

#include <cstdint>
#include "device.h"
#include "esp_err.h"

namespace command_status {
	// Prints the shared head of a peripheral status command.
	// Returns false when no sample exists yet, so the caller prints nothing else.
	bool header(Status status, int64_t updated_us, esp_err_t last_error, int64_t error_us);
	// Prints OK or FAIL <reason> for a write action. Returns the command exit code.
	int ack(esp_err_t result);
}
