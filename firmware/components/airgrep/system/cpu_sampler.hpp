#pragma once

#include "sdkconfig.h"

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#include "cpu_load.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace cpu {
using Sampler = Window<CONFIG_FREERTOS_NUMBER_OF_CORES>;

inline Sampler::Load sample(Sampler& sampler, uint64_t now) {
	return sampler.sample(now, ulTaskGetIdleRunTimeCounterForCore);
}
}
#endif
