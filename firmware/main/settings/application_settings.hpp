#pragma once

#include "settings/settings.hpp"

namespace application_settings {
#if AIRGREP_HAS_DISPLAY
esp_err_t erase_application_settings();
esp_err_t format_nvs_partition();
#else
using settings::erase_application_settings;
using settings::format_nvs_partition;
#endif
}
