#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// UART link to the ADSBee COMMS port: 115200 baud, 8E1, no flow control.
// Lines starting with '#' are CSBEE reports, everything else is an AT reply.

typedef void (*adsbee_line_cb)(const char* line, size_t len, bool csbee);

esp_err_t adsbee_link_start(adsbee_line_cb on_line);

// Appends LF. Keep one command outstanding.
esp_err_t adsbee_link_send(const char* command);
