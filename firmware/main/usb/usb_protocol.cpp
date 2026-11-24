#include "usb_protocol.hpp"

#include <cstring>

namespace usb::protocol {
namespace {
uint16_t read16(const uint8_t* bytes) {
	return static_cast<uint16_t>(bytes[0] | (unsigned{bytes[1]} << 8U));
}

void write_le(uint8_t* bytes, uint64_t value, size_t size) {
	for (size_t i = 0; i < size; i++) {
		bytes[i] = static_cast<uint8_t>(value);
		value >>= 8U;
	}
}

size_t finish_response(Command command, Status status, uint8_t* output,
					   size_t size) {
	output[0] = kVersion;
	write_le(output + 1, size, 2);
	output[3] = static_cast<uint8_t>(command);
	output[6] = static_cast<uint8_t>(status);
	uint16_t crc = crc16(output, 4);
	crc = crc16(output + 6, size - 6, crc);
	write_le(output + 4, crc, 2);
	return size;
}
}

uint16_t crc16(const uint8_t* bytes, size_t size, uint16_t seed) {
	unsigned crc = seed;
	for (size_t i = 0; i < size; i++) {
		crc ^= unsigned{bytes[i]} << 8U;
		for (unsigned bit = 0; bit < 8; bit++) {
			crc = (crc << 1U) ^ (crc & 0x8000U ? 0x1021U : 0U);
		}
	}
	return static_cast<uint16_t>(crc);
}

bool Decoder::timed_out(int64_t now_us) const {
	return received_ && !complete_ && now_us - started_us_ >= kTimeoutUs;
}

void Decoder::reset() { *this = Decoder{service_commands_}; }

bool Decoder::feed(uint8_t byte, int64_t now_us) {
	if (complete_ || timed_out(now_us)) {
		return false;
	}
	if (!received_) {
		started_us_ = now_us;
	}
	if (received_ < sizeof header_) {
		header_[received_] = byte;
	}
	if (received_ < 4 || received_ >= 6) {
		crc_ = crc16(&byte, 1, crc_);
	}
	received_++;
	if (received_ == 3) {
		length_ = read16(header_ + 1);
	}
	if (received_ < sizeof header_ || received_ < length_) {
		return false;
	}
	complete_ = true;
	if (length_ < sizeof header_) {
		status_ = Status::invalid_length;
	} else if (crc_ != read16(header_ + 4)) {
		status_ = Status::crc_mismatch;
	} else if (header_[0] != kVersion) {
		status_ = Status::version_mismatch;
	} else if (!service_commands_ && ((command() >= Command::read &&
									   command() <= Command::update_gnss) ||
									  (command() >= Command::list_traffic &&
									   command() <= Command::write_settings))) {
		status_ = Status::unknown_command;
	} else {
		switch (command()) {
			case Command::hello:
			case Command::reboot:
			case Command::enter_bootloader:
				status_ = length_ == sizeof header_ ? Status::ok
													: Status::invalid_length;
				break;
			case Command::read:
				status_ = length_ == 7 ? Status::ok : Status::invalid_length;
				break;
			case Command::list_traffic:
				status_ = length_ == 8 ? Status::ok : Status::invalid_length;
				break;
			case Command::write:
				status_ = length_ >= 11 ? Status::ok : Status::invalid_length;
				break;
			case Command::read_settings:
				status_ = length_ == 6 + kKeySize ? Status::ok
												  : Status::invalid_length;
				break;
			case Command::write_settings:
				// No payload removes every setting, otherwise a key and
				// its struct.
				status_ = length_ == 6 || length_ > 6 + kKeySize
							  ? Status::ok
							  : Status::invalid_length;
				break;
			case Command::update_traffic:
			case Command::update_gnss:
				status_ = length_ > 6 ? Status::ok : Status::invalid_length;
				break;
			default:
				status_ = Status::unknown_command;
		}
	}
	return true;
}

size_t encode_status(Status status, std::span<uint8_t> output) {
	return encode_response(Command::status, status, output, 0);
}

size_t encode_response(Command command, Status status,
					   std::span<uint8_t> output, size_t payload_size) {
	if (payload_size > UINT16_MAX - kStatusSize ||
		output.size() < kStatusSize + payload_size) {
		return 0;
	}
	return finish_response(command, status, output.data(),
						   kStatusSize + payload_size);
}

size_t encode_hello(const Hello& hello, std::span<uint8_t> output) {
	if (output.size() < kHelloSize) {
		return 0;
	}
	uint8_t* payload = output.data() + kStatusSize;
	write_le(payload, hello.uptime_seconds, 8);
	std::memcpy(payload + 8, hello.firmware_version, 32);
	std::memcpy(payload + 40, hello.variant, 32);
	write_le(payload + 72, hello.internal_free, 4);
	write_le(payload + 76, hello.internal_minimum, 4);
	write_le(payload + 80, hello.internal_total, 4);
	write_le(payload + 84, hello.psram_free, 4);
	write_le(payload + 88, hello.psram_minimum, 4);
	write_le(payload + 92, hello.psram_total, 4);
	write_le(payload + 96, hello.health, 2);
	payload[98] = hello.mode;
	payload[99] = hello.cpu_core0;
	payload[100] = hello.cpu_core1;
	payload[101] = hello.cpu_overall;
	payload[102] = static_cast<uint8_t>(hello.service);
	payload[103] = 0;
	std::memcpy(payload + 104, hello.firmware_description, 128);
	std::memcpy(payload + 232, hello.board, 128);
	std::memcpy(payload + 360, hello.chip, 128);
	return finish_response(Command::hello, Status::ok, output.data(),
						   kHelloSize);
}
}
