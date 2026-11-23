#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "adsbee/adsbee_protocol.hpp"
#include "charger/charge_target.hpp"
#include "system/power_mode.h"

// Events: facts a component publishes when its state changes, delivered to
// every subscriber of that type. Publish on the loop. Handlers run at once,
// in subscription order, and get nothing that was published before they
// subscribed. Nothing here touches the heap, the loop or the platform.
namespace events {
enum class Type : uint8_t {
	mode_changed,
	wifi_switch_changed,
	mute_switch_changed,
	health_changed,
	battery_low,
	battery_normal,
	barometer_reading,
	fix_acquired,
	fix_lost,
	clock_set,
	usb_plugged,
	usb_unplugged,
	usb_power_changed,
	supply_changed,
	charging_started,
	charging_stopped,
	charge_limit_changed,
	charger_protection_changed,
	adsbee_initialized,
	adsbee_reply_window_closed,
};

struct Mode {
	power_mode_t from, to;
};

struct Position {
	bool on;
};

struct Health {
	uint16_t previous_mask, current_mask;
};

struct Battery {
	float soc_percent;
};

struct Barometer {
	float pressure_hpa, temperature_c, altitude_m;
};

struct Fix {
	adsbee::AntennaStatus antenna;
};

struct Usb {
	uint16_t advertised_ma;  // 3000, 1500, or 0 for default current
};

struct Supply {
	charger::Supply source;
	uint16_t input_ma;
	bool operator==(const Supply&) const = default;
};

struct Charging {
	uint8_t phase;  // BQ25798 CHG_STAT
};

struct ChargeLimit {
	charger::ChargeStop stop;
	uint16_t charge_ma;
	bool operator==(const ChargeLimit&) const = default;
};

struct Protection {
	bool cold, cool, warm, hot;
	bool thermal_regulation, thermal_shutdown, fault;
	bool operator==(const Protection&) const = default;
};

class Event {
   private:
	union Data {
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
	Event(Type tag, Data payload) : type(tag), data(payload) {}

   public:
	const Type type;
	const Data data;

	static Event mode_changed(Mode value) {
		return {Type::mode_changed, {.mode = value}};
	}
	static Event wifi_switch_changed(Position value) {
		return {Type::wifi_switch_changed, {.position = value}};
	}
	static Event mute_switch_changed(Position value) {
		return {Type::mute_switch_changed, {.position = value}};
	}
	static Event health_changed(Health value) {
		return {Type::health_changed, {.health = value}};
	}
	static Event battery_changed(bool low, Battery value) {
		return {low ? Type::battery_low : Type::battery_normal,
				{.battery = value}};
	}
	static Event barometer_reading(Barometer value) {
		return {Type::barometer_reading, {.barometer = value}};
	}
	static Event fix_changed(bool acquired, Fix value) {
		return {acquired ? Type::fix_acquired : Type::fix_lost, {.fix = value}};
	}
	static Event usb_attachment_changed(bool attached, Usb value) {
		return {attached ? Type::usb_plugged : Type::usb_unplugged,
				{.usb = value}};
	}
	static Event usb_power_changed(Usb value) {
		return {Type::usb_power_changed, {.usb = value}};
	}
	static Event supply_changed(Supply value) {
		return {Type::supply_changed, {.supply = value}};
	}
	static Event charging_changed(bool started, Charging value) {
		return {started ? Type::charging_started : Type::charging_stopped,
				{.charging = value}};
	}
	static Event charge_limit_changed(ChargeLimit value) {
		return {Type::charge_limit_changed, {.charge_limit = value}};
	}
	static Event charger_protection_changed(Protection value) {
		return {Type::charger_protection_changed, {.protection = value}};
	}
	static Event clock_set() { return {Type::clock_set, {}}; }
	static Event adsbee_initialized() { return {Type::adsbee_initialized, {}}; }
	static Event adsbee_reply_window_closed() {
		return {Type::adsbee_reply_window_closed, {}};
	}
};

using Handler = void (*)(const Event&, void* context);

template <typename... Types>
constexpr uint32_t mask_for(Types... types) {
	return ((uint32_t{1} << uint8_t(types)) | ...);
}

template <typename T>
bool update_if_changed(T& last, const T& value) {
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

   private:
	struct Entry {
		std::atomic<Handler> handler{nullptr};  // set last, publishes the entry
		void* context{};
		uint32_t types{};
	};

	Entry entries_[kMaxSubscribers];
	std::atomic<size_t> claimed_{0};
};

bool subscribe(uint32_t types, Handler handler, void* context = nullptr);
void publish(const Event& event);
}
