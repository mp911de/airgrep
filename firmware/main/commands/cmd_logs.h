#pragma once
#include "esp_err.h"

esp_err_t logs_register(void);
esp_err_t logs_start(void);
void logs_stop(void);
