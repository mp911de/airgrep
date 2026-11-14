#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "charger/charge_target.hpp"
#include "system/power_mode.h"

// Events: facts a component publishes when its state changes, delivered to
// every subscriber of that type. Publish on the loop. Handlers run at once,
// in subscription order, and get nothing that was published before they
// subscribed. Nothing here touches the heap, the loop or the platform.
namespace events {
enum class Type : uint8_t {
	mode_changed,                // mode
	wifi_switch_changed,         // position
	mute_switch_changed,         // position
	health_changed,              // health
	battery_low,                 // battery
	battery_normal,              // battery
	barometer_reading,           // barometer
	fix_acquired,                // fix
	fix_lost,                    // fix, only after a fix was held
	clock_set,                   // no payload
	usb_plugged,                 // usb
	usb_unplugged,               // usb
	usb_power_changed,           // usb, the source changed its advertisement
	supply_changed,              // supply
	charging_started,            // charging
	charging_stopped,            // charging
	charge_limit_changed,        // charge_limit
	charger_protection_changed,  // protection
};

struct Mode {
	power_mode_t from, to;
};

struct Position {
	bool on;
};

// Health check masks, one health bit per component that is not usable.
struct Health {
	uint16_t before, now;
};

struct Battery {
	float soc;  // percent
};

struct Barometer {
	float pressure_hpa, temperature_c, altitude_m;
};

struct Fix {
	int8_t antenna;  // MON-RF: INIT DONTKNOW OK SHORT OPEN, -1 unknown
};

struct Usb {
	uint16_t advertised_ma;  // 3000, 1500, or 0 for default current
};

// What charge control feeds the charger from.
struct Supply {
	charger::Supply source;
	uint16_t input_ma;
	bool operator==(const Supply&) const = default;
};

struct Charging {
	uint8_t phase;  // BQ25798 CHG_STAT
};

// What charge control allows: its stop reason and the charge current.
struct ChargeLimit {
	charger::ChargeStop stop;
	uint16_t charge_ma;
	bool operator==(const ChargeLimit&) const = default;
};

// The charger's own protection: TS zone, die temperature and faults.
struct Protection {
	bool cold, cool, warm, hot;
	bool thermal_regulation, thermal_shutdown, fault;
	bool operator==(const Protection&) const = default;
};

struct Event {
	Type type;
	union {
		Mode mode;
		Position position;
		Health health;
		Battery battery;
		Barometer barometer;
		Fix fix;
		Usb usb;
		Supply supply;
		Charging charging;
		ChargeLimit charge_limit;
		Protection protection;
	};
};

// Fits the capture of a loop Task, so an event can cross to the loop later.
static_assert(sizeof(Event) <= 16);

using Handler = void (*)(const Event&, void* context);

// The set of types a subscriber wants.
template <typename... Types>
constexpr uint32_t of(Types... types) {
	return ((uint32_t{1} << uint8_t(types)) | ...);
}

// True when value differs from last, which then takes it. A flag that starts
// false reports its first true and never a leading false.
template <typename T>
bool changed(T& last, const T& value) {
	if (value == last) {
		return false;
	}
	last = value;
	return true;
}

inline constexpr size_t kMaxSubscribers = 16;

class Bus {
   public:
	// Any task subscribes. False when the bus is full.
	bool subscribe(uint32_t types, Handler handler, void* context = nullptr);
	void publish(const Event& event);
	// An event without payload.
	void publish(Type type);

   private:
	struct Entry {
		std::atomic<Handler> handler{nullptr};  // set last, publishes the entry
		void* context;
		uint32_t types;
	};

	Entry entries_[kMaxSubscribers];
	std::atomic<size_t> claimed_{0};
};

// The firmware's one bus.
bool subscribe(uint32_t types, Handler handler, void* context = nullptr);
void publish(const Event& event);
void publish(Type type);
}  // namespace events
