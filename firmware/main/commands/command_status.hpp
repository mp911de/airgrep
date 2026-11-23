#pragma once

#include <cstdint>

#include "device.h"
#include "esp_err.h"

namespace command_status {

void print_status(Status status, int64_t updated_us, esp_err_t last_error,
				  int64_t error_us);
int ack(esp_err_t result);
}
