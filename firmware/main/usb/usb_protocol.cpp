#include "usb_protocol.hpp"

#include <cstring>

namespace usb::protocol {
namespace {
uint16_t read16(const uint8_t* bytes) {
	return bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8);
}

void write_le(uint8_t* bytes, uint64_t value, size_t size) {
	for (size_t i = 0; i < size; i++) {
		bytes[i] = static_cast<uint8_t>(value);
		value >>= 8;
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
}  // namespace

uint16_t crc16(const uint8_t* bytes, size_t size, uint16_t seed) {
	for (size_t i = 0; i < size; i++) {
		seed ^= static_cast<uint16_t>(bytes[i]) << 8;
		for (unsigned bit = 0; bit < 8; bit++) {
			seed = static_cast<uint16_t>((seed << 1) ^
										 (seed & 0x8000 ? 0x1021 : 0));
		}
	}
	return seed;
}

bool Decoder::timed_out(int64_t now_us) const {
	return received_ && !complete_ && now_us - started_us_ >= kTimeoutUs;
}

void Decoder::reset() { *this = Decoder{}; }

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
			case Command::write:
				status_ = length_ >= 11 ? Status::ok : Status::invalid_length;
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

size_t encode_status(Status status, uint8_t* output) {
	return finish_response(Command::status, status, output, kStatusSize);
}

size_t encode_response(Command command, Status status, uint8_t* output,
					   size_t payload_size) {
	if (payload_size > UINT16_MAX - kStatusSize) {
		return 0;
	}
	return finish_response(command, status, output, kStatusSize + payload_size);
}

size_t encode_hello(const Hello& hello, uint8_t* output) {
	uint8_t* payload = output + kStatusSize;
	write_le(payload, hello.uptime_seconds, 8);
	std::memcpy(payload + 8, hello.firmware_version, 32);
	write_le(payload + 40, hello.internal_free, 4);
	write_le(payload + 44, hello.internal_minimum, 4);
	write_le(payload + 48, hello.internal_total, 4);
	write_le(payload + 52, hello.psram_free, 4);
	write_le(payload + 56, hello.psram_minimum, 4);
	write_le(payload + 60, hello.psram_total, 4);
	write_le(payload + 64, hello.health, 2);
	payload[66] = hello.mode;
	payload[67] = hello.cpu_core0;
	payload[68] = hello.cpu_core1;
	payload[69] = hello.cpu_overall;
	payload[70] = payload[71] = 0;
	return finish_response(Command::hello, Status::ok, output, kHelloSize);
}
}  // namespace usb::protocol
