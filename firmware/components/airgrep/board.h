#pragma once

#include "sdkconfig.h"

// Pin map of the fitted controller, see docs/interfaces.md.

#if CONFIG_AIRGREP_BOARD_DISPLAY
#define BOARD_NAME "Seeed Display 1.47"
#else
#define BOARD_NAME "S3 Plus"
#endif

#define BOARD_PIN_UART_RX CONFIG_AIRGREP_PIN_UART_RX
#define BOARD_PIN_UART_TX CONFIG_AIRGREP_PIN_UART_TX
#define BOARD_PIN_BUZZER CONFIG_AIRGREP_PIN_BUZZER
#define BOARD_PIN_SDA CONFIG_AIRGREP_PIN_SDA
#define BOARD_PIN_SCL CONFIG_AIRGREP_PIN_SCL
