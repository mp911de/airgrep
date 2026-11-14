#include "usb_components.hpp"

#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>

#include "adsbee/adsbee_clock.hpp"
#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "charger/bq25798.hpp"
#include "i2c_expander/tca9534.hpp"
#include "traffic/traffic.hpp"
#include "usb/fusb303.hpp"
#include "wifi/wifi_ap.hpp"

namespace usb {
namespace {
// Version 1 copies the S3 ABI, including padding and optional's representation.
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(bool) == 1 && sizeof(int) == 4 && sizeof(unsigned) == 4);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(std::optional<float>) == 8);
static_assert(alignof(std::optional<float>) == 4);
static_assert(sizeof(adsbee::Ingestion) == 1);
static_assert(sizeof(traffic::Source) == 1);
static_assert(sizeof(fusb::Role) == 4);
static_assert(TRAFFIC_MAX_TARGETS == 512);
static_assert(sizeof(traffic::Target) == 120);
static_assert(alignof(traffic::Target) == 8);
static_assert(std::is_trivially_copyable_v<traffic::Target>);
static_assert(std::is_standard_layout_v<traffic::Target>);
static_assert(sizeof(adsbee::Gnss) == 168);
static_assert(alignof(adsbee::Gnss) == 8);
static_assert(std::is_trivially_copyable_v<adsbee::Gnss>);
static_assert(std::is_standard_layout_v<adsbee::Gnss>);
static_assert(offsetof(traffic::Target, seen_us) == 8);
static_assert(offsetof(traffic::Target, latitude) == 36);
static_assert(offsetof(traffic::Target, frames) == 118);
static_assert(offsetof(adsbee::Gnss, epoch_ms) == 32);
static_assert(offsetof(adsbee::Gnss, module) == 100);
static_assert(offsetof(adsbee::Gnss, bbr) == 164);
static_assert(offsetof(adsbee::State, gnss) == 56);
static_assert(offsetof(adsbee::State, gnss_us) == 224);
static_assert(offsetof(adsbee::State, traffic_ingestion) == 232);
static_assert(offsetof(adsbee::State, gnss_ingestion) == 233);
static_assert(offsetof(barometer::State, pressure_hpa) == 52);
static_assert(offsetof(barometer::State, updated_us) == 64);
static_assert(offsetof(battery::State, charge_duration_hours) == 36);
static_assert(offsetof(battery::State, updated_us) == 48);
static_assert(offsetof(charger::State, updated_us) == 216);
static_assert(offsetof(charger::State, target) == 248);
static_assert(offsetof(expander::State, input) == 5);
static_assert(offsetof(expander::State, updated_us) == 40);
static_assert(offsetof(fusb::State, cc_limit_ma) == 44);
static_assert(offsetof(fusb::State, updated_us) == 48);
static_assert(offsetof(wifi::State, reported) == 5);
static_assert(offsetof(wifi::State, sent_us) == 104);
static_assert(offsetof(adsbee::timesync::State, synced_us) == 8);
static_assert(offsetof(adsbee::timesync::State, error_us) == 48);

template <typename State, size_t Size>
protocol::StateAccess state_access() {
	static_assert(sizeof(State) == Size);
	static_assert(alignof(State) == 8);
	static_assert(std::is_trivially_copyable_v<State>);
	static_assert(std::is_standard_layout_v<State>);
	return {sizeof(State), [](protocol::StateVisitor visit, void* context) {
				State::update([&](State& state) { visit(&state, context); });
				return true;
			}};
}

bool visit_traffic(protocol::StateVisitor visit, void* context) {
	struct Visit {
		protocol::StateVisitor action;
		void* context;
	} call{visit, context};
	return traffic::access(
		[](traffic::Target* slots, size_t, void* arg) {
			auto& visitor = *static_cast<Visit*>(arg);
			visitor.action(slots, visitor.context);
		},
		&call);
}

protocol::StateAccess find(uint8_t id) {
	switch (static_cast<Component>(id)) {
		case Component::adsbee:
			return state_access<adsbee::State, 240>();
		case Component::traffic:
			return {sizeof(traffic::Target) * TRAFFIC_MAX_TARGETS,
					visit_traffic};
		case Component::barometer:
			return state_access<barometer::State, 88>();
		case Component::battery:
			return state_access<battery::State, 72>();
		case Component::charger:
			return state_access<charger::State, 288>();
		case Component::expander:
			return state_access<expander::State, 64>();
		case Component::usb_c:
			return state_access<fusb::State, 72>();
		case Component::wifi:
			return state_access<wifi::State, 144>();
		case Component::adsbee_clock:
			return state_access<adsbee::timesync::State, 56>();
		default:
			return {};
	}
}

bool update_traffic(const uint8_t* payload, size_t count, int64_t now_us) {
	traffic::Target targets[32];
	if (!count || count > 32) {
		return false;
	}
	std::memcpy(targets, payload, count * sizeof(traffic::Target));
	return traffic::update(targets, count, now_us);
}

bool update_gnss(const uint8_t* payload, int64_t now_us) {
	adsbee::Gnss report;
	std::memcpy(&report, payload, sizeof report);
	adsbee::ingest_gnss(report, now_us);
	return true;
}
}  // namespace

const protocol::Services& component_services() {
	static const protocol::Services services{find, sizeof(traffic::Target),
											 sizeof(adsbee::Gnss),
											 update_traffic, update_gnss};
	return services;
}
}  // namespace usb
