#include "airgrep.h"

#include <cstring>
#include <memory>

#include "connection.hpp"
#include "device/dump.hpp"
#include "device/emulator.hpp"
#include "device/fixtures.hpp"
#include "device/usb.hpp"
#include "system/health.hpp"
#include "system/power_mode.h"

size_t airgrep_devices(airgrep_device* devices, size_t capacity,
					   char problem[256]) {
	size_t count = 0;
	problem[0] = 0;
	try {
		for (const device::usb_bus::Found& found : device::usb_bus::list()) {
			if (count == capacity) {
				break;
			}
			airgrep_device& out = devices[count++];
			device::put(out.serial, found.serial);
			out.data = found.has_data_interface;
		}
	} catch (const device::Error& error) {
		device::put(problem, 256, error.what());
	}
	return count;
}

airgrep_connection* airgrep_connect(const char* serial) {
	const std::string number = serial;
	return std::make_unique<airgrep_connection>(
			   number == AIRGREP_EMULATED
				   ? remote::Connection::Open(device::open_flight)
				   : [number] { return device::usb_bus::open(number); })
		.release();
}

airgrep_connection* airgrep_connect_fixture(const char* name) {
	const std::string fixture = name;
	const auto since_scene_ms = std::make_shared<std::atomic<int64_t>>(0);
	return std::make_unique<airgrep_connection>(
			   [fixture, since_scene_ms] {
				   return device::fixtures::open(fixture, since_scene_ms);
			   },
			   since_scene_ms)
		.release();
}

void airgrep_fixture_pass(airgrep_connection* connection, uint32_t elapsed_ms) {
	if (connection->fixture_ms) {
		*connection->fixture_ms += elapsed_ms;
	}
}

void airgrep_set_day(airgrep_connection* connection, int64_t utc_day) {
	connection->connection.set_day(
		std::chrono::sys_days{std::chrono::days{utc_day}});
}

void airgrep_disconnect(const airgrep_connection* connection) {
	const std::unique_ptr<const airgrep_connection> owned(connection);
}

void airgrep_status_of(const airgrep_connection* connection,
					   airgrep_status* status) {
	const remote::Connection::Status from = connection->connection.status();
	const device::Hello& hello = from.hello;
	*status = {};
	using Phase = remote::Connection::Phase;
	static_assert(static_cast<int>(Phase::connecting) == AIRGREP_CONNECTING &&
				  static_cast<int>(Phase::connected) == AIRGREP_CONNECTED &&
				  static_cast<int>(Phase::ended) == AIRGREP_ENDED);
	status->phase = static_cast<int>(from.phase);
	device::put(status->error, from.error);
	status->readable = from.supports_component_reads;
	status->uptime_seconds = hello.uptime_seconds;
	device::put(status->firmware, {hello.firmware_description,
								   strnlen(hello.firmware_description,
										   sizeof hello.firmware_description)});
	device::put(status->board,
				{hello.board, strnlen(hello.board, sizeof hello.board)});
	device::put(status->chip,
				{hello.chip, strnlen(hello.chip, sizeof hello.chip)});
	status->health = hello.health;
	health::format(hello.health, status->health_names,
				   sizeof status->health_names);
	device::put(status->mode,
				power_mode_name(static_cast<power_mode_t>(hello.mode)));
	status->cpu_core0 = hello.cpu_core0;
	status->cpu_core1 = hello.cpu_core1;
	status->cpu_overall = hello.cpu_overall;
	status->internal_free = hello.internal_free;
	status->internal_minimum = hello.internal_minimum;
	status->internal_total = hello.internal_total;
	status->psram_free = hello.psram_free;
	status->psram_minimum = hello.psram_minimum;
	status->psram_total = hello.psram_total;
	status->pictures_read = from.pictures;
	status->last_picture_read_ms =
		static_cast<double>(from.last_picture_read_time.count()) / 1000.0;
}

void airgrep_watch(airgrep_connection* connection, uint32_t interval_ms) {
	connection->connection.watch(std::chrono::milliseconds(interval_ms));
}

size_t airgrep_data_text(const airgrep_connection* connection, char* text,
						 size_t capacity) {
	device::DeviceState picture;
	if (!connection->connection.copy_latest_picture(picture)) {
		return 0;
	}
	const std::string whole = device::dump(picture);
	if (capacity) {
		device::put(text, capacity, whole);
	}
	return whole.size();
}

void airgrep_reboot(airgrep_connection* connection) {
	connection->connection.reboot();
}

void airgrep_enter_bootloader(airgrep_connection* connection) {
	connection->connection.enter_bootloader();
}

void airgrep_reset_settings(airgrep_connection* connection) {
	connection->connection.reset_settings();
}
