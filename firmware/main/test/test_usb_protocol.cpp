#include <cstring>
#include <initializer_list>

#include "unity.h"
#include "usb/usb_protocol.hpp"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[usb]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

using namespace usb::protocol;

static const uint8_t hello_request[] = {1, 6, 0, 1, 0xf5, 0x50};

static void set_crc(uint8_t* frame, size_t size) {
	const uint16_t crc = crc16(frame + 6, size - 6, crc16(frame, 4));
	frame[4] = static_cast<uint8_t>(crc);
	frame[5] = static_cast<uint8_t>(crc >> 8);
}

static void feed(Decoder& decoder, const uint8_t* frame, size_t size,
				 int64_t now_us = 100) {
	for (size_t i = 0; i < size; i++) {
		TEST_ASSERT_EQUAL(i + 1 == size, decoder.feed(frame[i], now_us));
	}
}

CASE(vectors, "USB CRC and status match the protocol's independent vectors") {
	const auto* digits = reinterpret_cast<const uint8_t*>("123456789");
	TEST_ASSERT_EQUAL_HEX16(0x29b1, crc16(digits, 9));
	TEST_ASSERT_EQUAL_HEX16(0x29b1, crc16(digits + 4, 5, crc16(digits, 4)));
	uint8_t output[kStatusSize];
	TEST_ASSERT_EQUAL_UINT(kStatusSize,
						   encode_status(Status::version_mismatch, output));
	const uint8_t expected[] = {1, 7, 0, 0x80, 0xaa, 0xd1, 2};
	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, output, sizeof expected);
}

CASE(hello, "USB HELLO completes only after all header bytes, at every split") {
	for (size_t split = 0; split <= sizeof hello_request; split++) {
		Decoder decoder;
		for (size_t i = 0; i < split; i++) {
			TEST_ASSERT_EQUAL(i + 1 == sizeof hello_request,
							  decoder.feed(hello_request[i], 10));
		}
		for (size_t i = split; i < sizeof hello_request; i++) {
			TEST_ASSERT_EQUAL(i + 1 == sizeof hello_request,
							  decoder.feed(hello_request[i], 1000));
		}
		TEST_ASSERT_EQUAL(Command::hello, decoder.command());
		TEST_ASSERT_EQUAL(Status::ok, decoder.status());
		TEST_ASSERT_FALSE(decoder.timed_out(kTimeoutUs * 2));
		TEST_ASSERT_FALSE(decoder.feed(1, kTimeoutUs * 2));
	}
}

CASE(reset_commands, "USB restart commands need valid frames, not HELLO") {
	uint8_t output[kHelloSize];
	for (auto command : {Command::reboot, Command::enter_bootloader}) {
		uint8_t frame[] = {kVersion, 6, 0, static_cast<uint8_t>(command), 0, 0};
		set_crc(frame, sizeof frame);
		Decoder decoder;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::ok, decoder.status());
		TEST_ASSERT_EQUAL(command, decoder.command());
		auto reply = route(decoder, output, [] { return Hello{}; });
		TEST_ASSERT_TRUE(reply.has_value());
		TEST_ASSERT_EQUAL(command, reply->after_send);
		TEST_ASSERT_EQUAL_UINT(kStatusSize, reply->size);
		TEST_ASSERT_EQUAL(Status::ok, output[6]);
		decoder.reset();
		frame[4] ^= 1;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::crc_mismatch, decoder.status());
		reply = route(decoder, output, [] { return Hello{}; });
		TEST_ASSERT_TRUE(reply.has_value());
		TEST_ASSERT_EQUAL(Command::status, reply->after_send);
		TEST_ASSERT_EQUAL(Status::crc_mismatch, output[6]);
	}
}

CASE(integrity, "USB CRC failure takes precedence over version and command") {
	for (const size_t changed : {size_t{0}, size_t{3}, size_t{4}, size_t{5}}) {
		uint8_t frame[sizeof hello_request];
		std::memcpy(frame, hello_request, sizeof frame);
		frame[changed] ^= 0x80;
		Decoder decoder;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::crc_mismatch, decoder.status());
	}
	uint8_t frame[] = {kVersion + 1, 6, 0, 0xfe, 0, 0};
	set_crc(frame, sizeof frame);
	Decoder decoder;
	feed(decoder, frame, sizeof frame);
	TEST_ASSERT_EQUAL(Status::version_mismatch, decoder.status());
}

CASE(unsupported, "USB unsupported commands use generic unknown status") {
	for (const uint8_t command : {0, 0x80, 0xff}) {
		uint8_t frame[] = {kVersion, 6, 0, command, 0, 0};
		set_crc(frame, sizeof frame);
		Decoder decoder;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::unknown_command, decoder.status());
	}
}

CASE(data_shapes,
	 "USB data commands check framing before dispatching payloads") {
	uint8_t frame[6 + kKeySize + 2] = {kVersion};
	uint8_t output[kHelloSize];
	for (auto command : {Command::read, Command::write, Command::update_traffic,
						 Command::update_gnss, Command::list_traffic,
						 Command::read_settings, Command::write_settings}) {
		for (size_t size = 6; size <= sizeof frame; size++) {
			frame[1] = static_cast<uint8_t>(size);
			frame[3] = static_cast<uint8_t>(command);
			set_crc(frame, size);
			Decoder decoder{true};
			feed(decoder, frame, size);
			const bool valid = command == Command::read           ? size == 7
							   : command == Command::list_traffic ? size == 8
							   : command == Command::write        ? size >= 11
							   : command == Command::read_settings
								   ? size == 6 + kKeySize
							   : command == Command::write_settings
								   ? size == 6 || size > 6 + kKeySize
								   : size > 6;
			TEST_ASSERT_EQUAL(valid ? Status::ok : Status::invalid_length,
							  decoder.status());
			auto reply = route(decoder, output, [] { return Hello{}; });
			TEST_ASSERT_EQUAL(!valid, reply.has_value());
			TEST_ASSERT_EQUAL_UINT(size, decoder.frame_length());
			decoder.reset();
			frame[4] ^= 1;
			feed(decoder, frame, size);
			TEST_ASSERT_EQUAL(Status::crc_mismatch, decoder.status());
		}
	}
}

CASE(
	service_gate,
	"USB service commands are disabled by default and stay disabled on reset") {
	uint8_t frame[6 + kKeySize] = {kVersion};
	Decoder decoder;
	for (auto command : {Command::read, Command::write, Command::update_traffic,
						 Command::update_gnss, Command::list_traffic,
						 Command::read_settings, Command::write_settings}) {
		frame[1] = command == Command::write            ? 12
				   : command == Command::list_traffic   ? 8
				   : command == Command::read_settings  ? sizeof frame
				   : command == Command::write_settings ? 6
														: 7;
		frame[3] = static_cast<uint8_t>(command);
		set_crc(frame, frame[1]);
		feed(decoder, frame, frame[1]);
		TEST_ASSERT_EQUAL(Status::unknown_command, decoder.status());
		decoder.reset();
		frame[4] ^= 1;
		feed(decoder, frame, frame[1]);
		TEST_ASSERT_EQUAL(Status::crc_mismatch, decoder.status());
		decoder.reset();
		frame[0] = kVersion + 1;
		set_crc(frame, frame[1]);
		feed(decoder, frame, frame[1]);
		TEST_ASSERT_EQUAL(Status::version_mismatch, decoder.status());
		decoder.reset();
		frame[0] = kVersion;
	}
	feed(decoder, hello_request, sizeof hello_request);
	TEST_ASSERT_EQUAL(Status::ok, decoder.status());
	uint8_t output[kHelloSize];
	auto reply = route(decoder, output, [] { return Hello{}; });
	TEST_ASSERT_TRUE(reply.has_value());
	TEST_ASSERT_EQUAL_UINT(kHelloSize, reply->size);
	TEST_ASSERT_EQUAL(Command::status, reply->after_send);
	TEST_ASSERT_EQUAL(Command::hello, output[3]);
}

CASE(lengths, "USB supported commands reject payloads after CRC validation") {
	for (auto command :
		 {Command::hello, Command::reboot, Command::enter_bootloader}) {
		uint8_t frame[] = {kVersion, 7, 0,   static_cast<uint8_t>(command),
						   0,        0, 0x55};
		set_crc(frame, sizeof frame);
		Decoder decoder;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::invalid_length, decoder.status());
		decoder.reset();
		frame[6] ^= 1;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::crc_mismatch, decoder.status());
	}
	for (uint8_t length = 0; length < 6; length++) {
		uint8_t frame[] = {kVersion, length, 0, 1, 0, 0};
		Decoder decoder;
		feed(decoder, frame, sizeof frame);
		TEST_ASSERT_EQUAL(Status::invalid_length, decoder.status());
	}
}

CASE(maximum, "USB consumes maximum frame length without retaining payload") {
	static uint8_t frame[65535];
	std::memset(frame, 0x55, sizeof frame);
	frame[0] = kVersion;
	frame[1] = frame[2] = 0xff;
	frame[3] = static_cast<uint8_t>(Command::hello);
	set_crc(frame, sizeof frame);
	Decoder decoder;
	feed(decoder, frame, sizeof frame);
	TEST_ASSERT_EQUAL(Status::invalid_length, decoder.status());
	TEST_ASSERT_LESS_THAN_UINT(64, sizeof decoder);
}

CASE(deadline, "USB byte trickles cannot extend the three second deadline") {
	Decoder decoder;
	TEST_ASSERT_FALSE(decoder.timed_out(kTimeoutUs * 10));
	TEST_ASSERT_FALSE(decoder.feed(kVersion, 0));
	TEST_ASSERT_FALSE(decoder.feed(6, 1000000));
	TEST_ASSERT_FALSE(decoder.feed(0, 2000000));
	TEST_ASSERT_FALSE(decoder.feed(1, kTimeoutUs - 1));
	TEST_ASSERT_FALSE(decoder.timed_out(kTimeoutUs - 1));
	TEST_ASSERT_TRUE(decoder.timed_out(kTimeoutUs));
	TEST_ASSERT_FALSE(decoder.feed(hello_request[4], kTimeoutUs));
	TEST_ASSERT_FALSE(decoder.feed(hello_request[5], kTimeoutUs + 1));
	TEST_ASSERT_TRUE(decoder.timed_out(kTimeoutUs + 1));
	decoder.reset();
	TEST_ASSERT_FALSE(decoder.timed_out(kTimeoutUs * 2));
	feed(decoder, hello_request, sizeof hello_request, kTimeoutUs * 2);
	TEST_ASSERT_EQUAL(Status::ok, decoder.status());
}

CASE(before_deadline, "USB complete frame before deadline remains usable") {
	Decoder decoder;
	TEST_ASSERT_FALSE(decoder.feed(kVersion, 100));
	for (size_t i = 1; i < sizeof hello_request; i++) {
		TEST_ASSERT_EQUAL(i + 1 == sizeof hello_request,
						  decoder.feed(hello_request[i], kTimeoutUs + 99));
	}
	TEST_ASSERT_FALSE(decoder.timed_out(kTimeoutUs + 100));
	TEST_ASSERT_EQUAL(Status::ok, decoder.status());
}

CASE(connection_reset, "USB connection reset discards an incomplete header") {
	Decoder decoder;
	TEST_ASSERT_FALSE(decoder.feed(0xff, 1));
	TEST_ASSERT_FALSE(decoder.feed(0xff, 2));
	decoder.reset();
	feed(decoder, hello_request, sizeof hello_request);
	TEST_ASSERT_EQUAL(Status::ok, decoder.status());
}

CASE(short_output, "USB encoders leave short output buffers untouched") {
	uint8_t storage[kHelloSize];
	std::memset(storage, 0xa5, sizeof storage);
	auto room = std::span(storage);
	TEST_ASSERT_EQUAL_UINT(
		0, encode_status(Status::ok, room.first(kStatusSize - 1)));
	TEST_ASSERT_EQUAL_UINT(0,
						   encode_hello(Hello{}, room.first(kHelloSize - 1)));
	TEST_ASSERT_EQUAL_UINT(0, encode_response(Command::read, Status::ok, room,
											  room.size() - kStatusSize + 1));
	TEST_ASSERT_EQUAL_UINT(
		0, encode_response(Command::read, Status::ok, room, SIZE_MAX));
	TEST_ASSERT_EACH_EQUAL_UINT8(0xa5, storage, sizeof storage);
}

CASE(hello_response, "USB HELLO encodes all fields at their declared offsets") {
	Hello sample{};
	sample.uptime_seconds = 0x0807060504030201ULL;
	std::strcpy(sample.firmware_version, "0.1.0");
	std::strcpy(sample.firmware_description,
				"airgrep 0.1.0 (Nov 15 2026 14:23:45)");
	std::strcpy(sample.board, "S3 Plus");
	std::strcpy(sample.chip, "ESP32-S3 (QFN56) rev v0.2, 2 cores @ 80 MHz");
	sample.internal_free = 0x44332211;
	sample.internal_minimum = 0x55443322;
	sample.internal_total = 0x66554433;
	sample.psram_free = 0x77665544;
	sample.psram_minimum = 0x88776655;
	sample.psram_total = 0x99887766;
	sample.health = 0x0302;
	sample.mode = 2;
	sample.cpu_core0 = 10;
	sample.cpu_core1 = 20;
	sample.cpu_overall = 15;
	sample.service = Service::present;
	sample.reserved = 0xff;
	uint8_t output[kHelloSize];
	TEST_ASSERT_EQUAL_UINT(kHelloSize, encode_hello(sample, output));
	TEST_ASSERT_EQUAL_UINT8(kVersion, output[0]);
	TEST_ASSERT_EQUAL_UINT8(kHelloSize & 0xff, output[1]);
	TEST_ASSERT_EQUAL_UINT8(kHelloSize >> 8, output[2]);
	TEST_ASSERT_EQUAL_UINT8(1, output[3]);
	TEST_ASSERT_EQUAL_UINT8(0, output[6]);
	const uint8_t uptime[] = {1, 2, 3, 4, 5, 6, 7, 8};
	TEST_ASSERT_EQUAL_HEX8_ARRAY(uptime, output + 7, 8);
	TEST_ASSERT_EQUAL_STRING("0.1.0", output + 15);
	TEST_ASSERT_EQUAL_MEMORY(sample.firmware_version, output + 15, 32);
	const uint8_t tail[] = {
		0x11, 0x22, 0x33, 0x44, 0x22, 0x33, 0x44, 0x55, 0x33, 0x44, 0x55,
		0x66, 0x44, 0x55, 0x66, 0x77, 0x55, 0x66, 0x77, 0x88, 0x66, 0x77,
		0x88, 0x99, 0x02, 0x03, 2,    10,   20,   15,   2,    0,
	};
	TEST_ASSERT_EQUAL_HEX8_ARRAY(tail, output + 47, sizeof tail);
	TEST_ASSERT_EQUAL_STRING("airgrep 0.1.0 (Nov 15 2026 14:23:45)",
							 output + 79);
	TEST_ASSERT_EQUAL_MEMORY(sample.firmware_description, output + 79, 128);
	TEST_ASSERT_EQUAL_MEMORY(sample.board, output + 207, 128);
	TEST_ASSERT_EQUAL_MEMORY(sample.chip, output + 335, 128);
	const uint16_t expected_crc =
		crc16(output + 6, sizeof output - 6, crc16(output, 4));
	TEST_ASSERT_EQUAL_HEX16(expected_crc, output[4] | (output[5] << 8));
}

CASE(unsupported_version,
	 "USB unsupported version receives the current version in its error "
	 "response") {
	uint8_t request[] = {kVersion + 1, 6, 0, 1, 0, 0};
	set_crc(request, sizeof request);
	Decoder decoder;
	feed(decoder, request, sizeof request);
	TEST_ASSERT_EQUAL(Status::version_mismatch, decoder.status());
	uint8_t response[kStatusSize];
	TEST_ASSERT_EQUAL_UINT(kStatusSize,
						   encode_status(decoder.status(), response));
	const uint8_t expected[] = {1, 7, 0, 0x80, 0xaa, 0xd1, 2};
	TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, response, sizeof expected);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(vectors);
	RUN_TEST(hello);
	RUN_TEST(reset_commands);
	RUN_TEST(integrity);
	RUN_TEST(unsupported);
	RUN_TEST(data_shapes);
	RUN_TEST(service_gate);
	RUN_TEST(lengths);
	RUN_TEST(maximum);
	RUN_TEST(deadline);
	RUN_TEST(before_deadline);
	RUN_TEST(connection_reset);
	RUN_TEST(short_output);
	RUN_TEST(hello_response);
	RUN_TEST(unsupported_version);
	return UNITY_END();
}
#endif
