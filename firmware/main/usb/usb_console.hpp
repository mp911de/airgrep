#pragma once

#include "esp_err.h"

esp_err_t usb_console_init();
void usb_console_stop();
void usb_console_set_service_task();
