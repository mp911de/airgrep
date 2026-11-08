#pragma once

#include "esp_log_level.h"

void log_session_begin(esp_log_level_t level, const char* tags);
void log_session_end(void);
