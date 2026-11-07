#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// IDF console REPL on the USB Serial/JTAG port with "help" and
// "log_level <tag|*> <none|error|warn|info|debug|verbose>".

esp_err_t console_start(void);

#ifdef __cplusplus
}
#endif
