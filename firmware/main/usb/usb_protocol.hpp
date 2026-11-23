#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "usb_constants.h"

namespace usb::protocol {

inline constexpr uint8_t kVersion = 1;
inline constexpr int64_t kTimeoutUs = 3000000;
inline constexpr size_t kStatusSize = 7;
inline constexpr size_t kHelloSize = 463;
inline constexpr size_t kListCountsSize = 4, kListPresentOffset = 0,
						kListReturnedOffset = 2;
// A settings key travels as this many bytes: its characters, its end and
// zeros. It is the key in NVS, which takes 15 characters.
inline constexpr size_t kKeySize = 16;

enum class Command : uint8_t {
	hello = 0x01,
	read = 0x02,
	write = 0x03,
	update_traffic = 0x04,
	update_gnss = 0x05,
	reboot = 0x06,
	enter_bootloader = 0x07,
	list_traffic = 0x08,
	read_settings = 0x09,
	write_settings = 0x0a,
	status = 0x80,
};

constexpr Command success_response(Command command) {
	switch (command) {
		case Command::hello:
		case Command::read:
		case Command::list_traffic:
		case Command::read_settings:
			return command;
		default:
			return Command::status;
	}
}

enum class Status : uint8_t {
	ok,
	crc_mismatch,
	version_mismatch,
	unknown_command,
	unknown_component,
	invalid_length,
	invalid_range,
	internal_error,

	unknown_settings,
};

// Whether READ, WRITE, the UPDATE and the SETTINGS commands exist in this
// firmware. Firmware older than the field reports unknown, a client then has
// to try one.
enum class Service : uint8_t { unknown, absent, present };

struct Hello {
	uint64_t uptime_seconds;
	char firmware_version[32];
	uint32_t internal_free, internal_minimum, internal_total;
	uint32_t psram_free, psram_minimum, psram_total;
	uint16_t health;
	uint8_t mode, cpu_core0, cpu_core1, cpu_overall;
	Service service;
	uint8_t reserved;
	char firmware_description[128];
	char board[128];
	char chip[128];
};

static_assert(sizeof(Hello) == 456);
static_assert(kHelloSize == kStatusSize + sizeof(Hello));
static_assert(offsetof(Hello, uptime_seconds) == 0);
static_assert(offsetof(Hello, firmware_version) == 8);
static_assert(offsetof(Hello, internal_free) == 40);
static_assert(offsetof(Hello, internal_minimum) == 44);
static_assert(offsetof(Hello, internal_total) == 48);
static_assert(offsetof(Hello, psram_free) == 52);
static_assert(offsetof(Hello, psram_minimum) == 56);
static_assert(offsetof(Hello, psram_total) == 60);
static_assert(offsetof(Hello, health) == 64);
static_assert(offsetof(Hello, mode) == 66);
static_assert(offsetof(Hello, cpu_core0) == 67);
static_assert(offsetof(Hello, cpu_core1) == 68);
static_assert(offsetof(Hello, cpu_overall) == 69);
static_assert(offsetof(Hello, service) == 70);
static_assert(offsetof(Hello, reserved) == 71);
static_assert(offsetof(Hello, firmware_description) == 72);
static_assert(offsetof(Hello, board) == 200);
static_assert(offsetof(Hello, chip) == 328);

uint16_t crc16(const uint8_t* bytes, size_t size, uint16_t seed = 0xffff);

// Integrity checking streams over the frame. The transport owns payload storage
// and only dispatches it after this decoder completes with Status::ok.
class Decoder {
   public:
	explicit Decoder(bool service_commands = false) noexcept
		: service_commands_(service_commands) {}
	bool feed(uint8_t byte, int64_t now_us);
	bool timed_out(int64_t now_us) const;
	void reset();
	Command command() const { return static_cast<Command>(header_[3]); }
	Status status() const { return status_; }
	uint16_t frame_length() const { return length_; }

   private:
	uint8_t header_[6]{};
	uint32_t received_ = 0;
	uint16_t length_ = 0, crc_ = 0xffff;
	int64_t started_us_ = 0;
	Status status_ = Status::internal_error;
	bool complete_ = false;
	bool service_commands_;
};

size_t encode_status(Status status, std::span<uint8_t> output);
size_t encode_hello(const Hello& hello, std::span<uint8_t> output);

size_t encode_response(Command command, Status status,
					   std::span<uint8_t> output, size_t payload_size);

struct Reply {
	size_t size;
	Command after_send = Command::status;
};

// A completed frame either has an immediate reply or needs a data service.
// A reset takes effect after its acknowledgement reaches the host.
inline std::optional<Reply> route(const Decoder& request,
								  std::span<uint8_t> output, auto&& hello) {
	if (request.status() != Status::ok) {
		return Reply{encode_status(request.status(), output)};
	}
	switch (request.command()) {
		case Command::hello:
			return Reply{encode_hello(hello(), output)};
		case Command::reboot:
		case Command::enter_bootloader:
			return Reply{encode_status(Status::ok, output), request.command()};
		default:
			return std::nullopt;
	}
}

}
