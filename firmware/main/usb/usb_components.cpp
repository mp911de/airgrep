#include "usb_components.hpp"

#include <cstring>
#include <span>
#include <type_traits>

#include "adsbee/adsbee_clock.hpp"
#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "charger/bq25798.hpp"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "i2c_expander/tca9534.hpp"
#include "nvs.h"
#include "settings/application_settings.hpp"
#include "settings/settings.hpp"
#include "traffic/traffic.hpp"
#include "usb/fusb303.hpp"
#include "usb_layout.hpp"
#include "usb_settings.hpp"
#include "wifi/wifi_ap.hpp"

#if AIRGREP_HAS_DISPLAY
#include "display/active_display_settings.hpp"
#endif

namespace usb {
namespace {
static_assert(TRAFFIC_MAX_TARGETS == layout::kTargets);
static_assert(traffic::kListed ==
			  (UINT16_MAX - protocol::kStatusSize - protocol::kListCountsSize) /
				  sizeof(traffic::Tracked));

template <typename State>
protocol::StateAccess state_access() {
	static_assert(layout::kExposed<State, sizeof(typename State::StateData)>);
	return {sizeof(State), nullptr,
			[](void*, std::span<uint8_t> out) {
				State::update([&](State& state) {
					std::memcpy(out.data(), &state, out.size());
				});
				return true;
			},
			[](void*, size_t offset, std::span<const uint8_t> bytes) {
				State::update([&](State& state) {
					std::memcpy(reinterpret_cast<uint8_t*>(&state) + offset,
								bytes.data(), bytes.size());
				});
				return true;
			}};
}

protocol::StateAccess find(void*, uint8_t id) {
	switch (static_cast<Component>(id)) {
		case Component::adsbee:
			return state_access<adsbee::State>();
		case Component::traffic:
			return {sizeof(traffic::Target) * TRAFFIC_MAX_TARGETS, nullptr,
					[](void*, std::span<uint8_t> out) {
						return traffic::read_slots(out);
					},
					[](void*, size_t offset, std::span<const uint8_t> bytes) {
						return traffic::write_slots(offset, bytes);
					}};
		case Component::barometer:
			return state_access<barometer::State>();
		case Component::battery:
			return state_access<battery::State>();
		case Component::charger:
			return state_access<charger::State>();
		case Component::expander:
			return state_access<expander::State>();
		case Component::usb_c:
			return state_access<fusb::State>();
		case Component::wifi:
			return state_access<wifi::State>();
		case Component::adsbee_clock:
			return state_access<adsbee::timesync::State>();
		default:
			return {};
	}
}

bool update_traffic(void*, const uint8_t* payload, size_t count,
					int64_t now_us) {
	traffic::Target targets[32];
	if (!count || count > 32) {
		return false;
	}
	std::memcpy(targets, payload, count * sizeof(traffic::Target));
	for (size_t i = 0; i < count; i++) {
		targets[i].callsign[sizeof targets[i].callsign - 1] = 0;
	}
	return traffic::update(targets, count, now_us);
}

std::optional<protocol::ListedTraffic> list_traffic(void*, uint8_t* out,
													size_t limit) {
	static traffic::Tracked* listed = nullptr;
	if (limit && !listed) {
		listed = static_cast<traffic::Tracked*>(
			heap_caps_malloc(sizeof(traffic::Tracked) * traffic::kListed,
							 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
		if (!listed) {
			return std::nullopt;
		}
	}
	const adsbee::State link = adsbee::State::copy();
	const auto& gnss = link.gnss_at(esp_timer_get_time());
	traffic::Reference own{gnss.latitude, gnss.longitude};
	uint16_t present = 0;
	auto count = traffic::nearest(
		gnss.fix != adsbee::Fix::none ? &own : nullptr, listed, limit, present);
	if (count && *count) {
		std::memcpy(out, listed, *count * sizeof(traffic::Tracked));
	}
	if (!count) {
		return std::nullopt;
	}
	return protocol::ListedTraffic{present, *count};
}

bool update_gnss(void*, const uint8_t* payload, int64_t now_us) {
	adsbee::Gnss report;
	std::memcpy(&report, payload, sizeof report);
	for (const std::span<char> text :
		 {std::span<char>(report.sbas_mode),
		  std::span<char>(report.sbas_system), std::span<char>(report.module),
		  std::span<char>(report.software), std::span<char>(report.hardware)}) {
		text.back() = 0;
	}
	adsbee::ingest_gnss(report, now_us);
	return true;
}
#if AIRGREP_HAS_DISPLAY
constexpr protocol::Stored display_settings() {
	auto schema = protocol::stored<display::Settings>(display::kSettingsKey);
	schema.load = [](void*, const char*, uint8_t* out, size_t) {
		auto settings = display_settings::snapshot();
		std::memcpy(out, &settings, sizeof settings);
		return true;
	};
	schema.save = [](void*, const char*, const uint8_t* blob, size_t) {
		display::Settings settings;
		std::memcpy(&settings, blob, sizeof settings);
		return display_settings::save(settings);
	};
	return schema;
}
#endif

constexpr protocol::Stored kStored[] = {
	kSettings,
#if AIRGREP_HAS_DISPLAY
	display_settings(),
#endif
};
static_assert(protocol::kKeySize == NVS_KEY_NAME_MAX_SIZE);

bool load_settings(void*, const char* key, uint8_t* out, size_t size) {
	return settings::read(key, out, size) == ESP_OK;
}

bool store_settings(void*, const char* key, const uint8_t* blob, size_t size) {
	return settings::write(key, blob, size) == ESP_OK;
}

bool erase_settings(void*) {
	return application_settings::erase_application_settings() == ESP_OK;
}
}

const protocol::Services& component_services() {
	static const protocol::Services services{nullptr,
											 find,
											 sizeof(traffic::Target),
											 sizeof(adsbee::Gnss),
											 update_traffic,
											 update_gnss,
											 sizeof(traffic::Tracked),
											 list_traffic,
											 kStored,
											 load_settings,
											 store_settings,
											 erase_settings};
	return services;
}
}
