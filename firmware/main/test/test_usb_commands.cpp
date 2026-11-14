#include <cstring>
#include <initializer_list>

#include "unity.h"
#include "usb/usb_commands.hpp"
#include "usb/usb_protocol.hpp"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[usb]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

using namespace usb::protocol;

namespace {
struct State {
	uint32_t marker;
	uint8_t bytes[9];
};

State state;
size_t lookups, visits, updates, target_count;
int64_t received_us;
bool accessible;
const uint8_t* supplied;

bool visit(StateVisitor visitor, void* context) {
	if (!accessible) {
		return false;
	}
	visits++;
	visitor(&state, context);
	return true;
}

StateAccess find(uint8_t component) {
	lookups++;
	if (component == 1) {
		return {sizeof state, visit};
	}
	if (component == 2) {
		return {65535, visit};
	}
	return {};
}

bool traffic(const uint8_t* payload, size_t count, int64_t now_us) {
	if (!accessible) {
		return false;
	}
	updates++;
	supplied = payload;
	target_count = count;
	received_us = now_us;
	return true;
}

bool gnss(const uint8_t* payload, int64_t now_us) {
	return traffic(payload, 1, now_us);
}

const Services services{find, 120, 64, traffic, gnss};

void reset() {
	std::memset(&state, 0x55, sizeof state);
	lookups = visits = updates = target_count = 0;
	received_us = 0;
	accessible = true;
	supplied = nullptr;
}

void assert_frame(const uint8_t* response, size_t size, Command command,
				  Status status) {
	TEST_ASSERT_EQUAL_UINT8(kVersion, response[0]);
	TEST_ASSERT_EQUAL_UINT(size, response[1] | (response[2] << 8));
	TEST_ASSERT_EQUAL(command, response[3]);
	TEST_ASSERT_EQUAL(status, response[6]);
	TEST_ASSERT_EQUAL_HEX16(crc16(response + 6, size - 6, crc16(response, 4)),
							response[4] | (response[5] << 8));
}

void assert_status(Command command, const uint8_t* payload, size_t size,
				   Status expected) {
	uint8_t response[kStatusSize];
	TEST_ASSERT_EQUAL_UINT(
		kStatusSize, execute_data(command, payload, size, 123456, services,
								  response, sizeof response));
	assert_frame(response, sizeof response, Command::status, expected);
}
}  // namespace

CASE(read, "USB READ returns every State byte, including padding") {
	reset();
	uint8_t request[] = {1};
	uint8_t response[kStatusSize + sizeof state];
	TEST_ASSERT_EQUAL_UINT(
		sizeof response, execute_data(Command::read, request, sizeof request, 0,
									  services, response, sizeof response));
	assert_frame(response, sizeof response, Command::read, Status::ok);
	TEST_ASSERT_EQUAL_UINT(1, visits);
	TEST_ASSERT_EQUAL_MEMORY(&state, response + kStatusSize, sizeof state);
	std::memset(&state, 0xaa, sizeof state);
	for (size_t i = kStatusSize; i < sizeof response; i++) {
		TEST_ASSERT_EQUAL_HEX8(0x55, response[i]);
	}
}

CASE(partial_write, "USB WRITE copies the same byte range from a full State") {
	reset();
	uint8_t request[5 + sizeof state] = {1, 3, 0, 5, 0};
	for (size_t i = 0; i < sizeof state; i++) {
		request[5 + i] = static_cast<uint8_t>(i);
	}
	assert_status(Command::write, request, sizeof request, Status::ok);
	TEST_ASSERT_EQUAL_UINT(1, visits);
	const auto* actual = reinterpret_cast<const uint8_t*>(&state);
	for (size_t i = 0; i < sizeof state; i++) {
		TEST_ASSERT_EQUAL_HEX8(i >= 3 && i < 8 ? i : 0x55, actual[i]);
	}
	TEST_ASSERT_EQUAL_UINT(0, updates);
}

CASE(write_bounds, "USB WRITE rejects ranges without touching State") {
	reset();
	uint8_t request[5 + sizeof state] = {1};
	const uint16_t ranges[][2] = {
		{sizeof state + 1, 0}, {sizeof state, 1}, {1, sizeof state},
		{65535, 65535},        {0, 65535},
	};
	for (const auto& range : ranges) {
		request[1] = static_cast<uint8_t>(range[0]);
		request[2] = static_cast<uint8_t>(range[0] >> 8);
		request[3] = static_cast<uint8_t>(range[1]);
		request[4] = static_cast<uint8_t>(range[1] >> 8);
		assert_status(Command::write, request, sizeof request,
					  Status::invalid_range);
	}
	TEST_ASSERT_EQUAL_UINT(0, visits);
	request[1] = sizeof state;
	request[2] = request[3] = request[4] = 0;
	assert_status(Command::write, request, sizeof request, Status::ok);
	TEST_ASSERT_EQUAL_UINT(0, visits);
}

CASE(write_shape, "USB WRITE requires the complete State and no size field") {
	reset();
	uint8_t request[6 + sizeof state] = {1, 0, 0, 1, 0};
	for (size_t size : {size_t{0}, size_t{4}, size_t{5}, sizeof request - 2,
						sizeof request}) {
		assert_status(Command::write, request, size, Status::invalid_length);
	}
	TEST_ASSERT_EQUAL_UINT(0, visits);
}

CASE(component_errors, "USB component errors produce only generic status") {
	reset();
	uint8_t request[5 + sizeof state] = {99};
	assert_status(Command::read, request, 1, Status::unknown_component);
	assert_status(Command::write, request, sizeof request,
				  Status::unknown_component);
	request[0] = 1;
	assert_status(Command::read, request, 0, Status::invalid_length);
	assert_status(Command::read, request, 2, Status::invalid_length);
	accessible = false;
	uint8_t response[kStatusSize + sizeof state];
	TEST_ASSERT_EQUAL_UINT(kStatusSize,
						   execute_data(Command::read, request, 1, 0, services,
										response, sizeof response));
	assert_frame(response, kStatusSize, Command::status,
				 Status::internal_error);
	request[3] = 1;
	assert_status(Command::write, request, sizeof request,
				  Status::internal_error);
	TEST_ASSERT_EQUAL_UINT(0, visits);
}

CASE(capacity, "USB output capacity is checked before State access") {
	reset();
	uint8_t request[] = {1};
	uint8_t response[kStatusSize + sizeof state];
	std::memset(response, 0xaa, sizeof response);
	TEST_ASSERT_EQUAL_UINT(
		0, execute_data(Command::read, request, 1, 0, services, response, 6));
	TEST_ASSERT_EQUAL_HEX8(0xaa, response[0]);
	TEST_ASSERT_EQUAL_UINT(kStatusSize,
						   execute_data(Command::read, request, 1, 0, services,
										response, sizeof response - 1));
	assert_frame(response, kStatusSize, Command::status,
				 Status::internal_error);
	request[0] = 2;
	assert_status(Command::read, request, 1, Status::internal_error);
	TEST_ASSERT_EQUAL_UINT(0, visits);
}

CASE(traffic_batches, "USB Traffic accepts 1 and 32 targets as one batch") {
	reset();
	static uint8_t reports[120 * 33];
	for (size_t count : {size_t{1}, size_t{32}}) {
		updates = 0;
		assert_status(Command::update_traffic, reports, count * 120,
					  Status::ok);
		TEST_ASSERT_EQUAL_UINT(1, updates);
		TEST_ASSERT_EQUAL_UINT(count, target_count);
		TEST_ASSERT_EQUAL_PTR(reports, supplied);
		TEST_ASSERT_EQUAL_INT64(123456, received_us);
	}
	updates = 0;
	for (size_t size : {size_t{0}, size_t{119}, size_t{121}, sizeof reports}) {
		assert_status(Command::update_traffic, reports, size,
					  Status::invalid_length);
	}
	TEST_ASSERT_EQUAL_UINT(0, updates);
	accessible = false;
	assert_status(Command::update_traffic, reports, 120,
				  Status::internal_error);
	TEST_ASSERT_EQUAL_UINT(0, updates);
}

CASE(gnss_report, "USB GNSS forwards one complete report and receipt time") {
	reset();
	uint8_t report[65]{};
	for (size_t size : {size_t{0}, size_t{63}, size_t{65}}) {
		assert_status(Command::update_gnss, report, size,
					  Status::invalid_length);
	}
	TEST_ASSERT_EQUAL_UINT(0, updates);
	assert_status(Command::update_gnss, report, 64, Status::ok);
	TEST_ASSERT_EQUAL_UINT(1, updates);
	TEST_ASSERT_EQUAL_PTR(report, supplied);
	TEST_ASSERT_EQUAL_INT64(123456, received_us);
	accessible = false;
	assert_status(Command::update_gnss, report, 64, Status::internal_error);
	TEST_ASSERT_EQUAL_UINT(1, updates);
}

CASE(frame_limit,
	 "USB executor rejects payloads beyond the wire length limit") {
	reset();
	uint8_t byte = 1;
	assert_status(Command::write, &byte, 65530, Status::invalid_length);
	assert_status(Command::status, &byte, 1, Status::unknown_command);
	TEST_ASSERT_EQUAL_UINT(0, visits);
	TEST_ASSERT_EQUAL_UINT(0, updates);
	uint8_t response[kStatusSize];
	std::memset(response, 0xaa, sizeof response);
	TEST_ASSERT_EQUAL_UINT(
		0, encode_response(Command::read, Status::ok, response, 65529));
	TEST_ASSERT_EQUAL_HEX8(0xaa, response[0]);
}

#ifndef ESP_PLATFORM
namespace {
constexpr size_t traffic_bytes = 512 * 120;
uint8_t traffic_state[traffic_bytes];
uint8_t traffic_write[11 + traffic_bytes];
uint8_t large_response[65535];

bool visit_traffic(StateVisitor visitor, void* context) {
	visits++;
	visitor(traffic_state, context);
	return true;
}

StateAccess find_traffic(uint8_t component) {
	lookups++;
	return component == 2 ? StateAccess{sizeof traffic_state, visit_traffic}
						  : StateAccess{};
}

const Services traffic_services{find_traffic, 120, 64, traffic, gnss};

void crc_frame(uint8_t* frame, size_t size) {
	uint16_t crc = crc16(frame + 6, size - 6, crc16(frame, 4));
	frame[4] = static_cast<uint8_t>(crc);
	frame[5] = static_cast<uint8_t>(crc >> 8);
}

void prepare_traffic_write() {
	reset();
	std::memset(traffic_state, 0x55, sizeof traffic_state);
	std::memset(large_response, 0xaa, sizeof large_response);
	traffic_write[0] = kVersion;
	traffic_write[1] = static_cast<uint8_t>(sizeof traffic_write);
	traffic_write[2] = static_cast<uint8_t>(sizeof traffic_write >> 8);
	traffic_write[3] = static_cast<uint8_t>(Command::write);
	traffic_write[6] = 2;
	traffic_write[7] = 19;
	traffic_write[8] = 0;
	traffic_write[9] = static_cast<uint8_t>(61400);
	traffic_write[10] = static_cast<uint8_t>(61400 >> 8);
	for (size_t i = 0; i < traffic_bytes; i++) {
		traffic_write[11 + i] = static_cast<uint8_t>(i * 7);
	}
	crc_frame(traffic_write, sizeof traffic_write);
}

size_t receive(Decoder& decoder, uint8_t* frame, size_t index) {
	if (!decoder.feed(frame[index], static_cast<int64_t>(index * 17))) {
		return 0;
	}
	if (decoder.status() != Status::ok) {
		return encode_status(decoder.status(), large_response);
	}
	return execute_data(decoder.command(), frame + 6,
						decoder.frame_length() - 6, 123456, traffic_services,
						large_response, sizeof large_response);
}
}  // namespace

CASE(full_traffic_write,
	 "USB full Traffic WRITE waits for complete validation before copying") {
	prepare_traffic_write();
	Decoder decoder{true};
	for (size_t i = 0; i < sizeof traffic_write - 1; i++) {
		TEST_ASSERT_EQUAL_UINT(0, receive(decoder, traffic_write, i));
		if (i % 257 == 0 || i + 2 == sizeof traffic_write) {
			TEST_ASSERT_EQUAL_UINT(0, visits);
			TEST_ASSERT_EACH_EQUAL_UINT8(0x55, traffic_state,
										 sizeof traffic_state);
			TEST_ASSERT_EQUAL_HEX8(0xaa, large_response[0]);
		}
	}
	TEST_ASSERT_EQUAL_UINT(
		kStatusSize, receive(decoder, traffic_write, sizeof traffic_write - 1));
	assert_frame(large_response, kStatusSize, Command::status, Status::ok);
	TEST_ASSERT_EQUAL_UINT(1, visits);
	TEST_ASSERT_EACH_EQUAL_UINT8(0x55, traffic_state, 19);
	TEST_ASSERT_EQUAL_MEMORY(traffic_write + 11 + 19, traffic_state + 19,
							 61400);
	TEST_ASSERT_EACH_EQUAL_UINT8(0x55, traffic_state + 61419,
								 sizeof traffic_state - 61419);
}

CASE(rejected_traffic_write,
	 "USB CRC and version failures cannot reach Traffic State access") {
	for (Status expected : {Status::crc_mismatch, Status::version_mismatch}) {
		prepare_traffic_write();
		if (expected == Status::crc_mismatch) {
			traffic_write[sizeof traffic_write - 1] ^= 1;
		} else {
			traffic_write[0] = kVersion + 1;
			crc_frame(traffic_write, sizeof traffic_write);
		}
		Decoder decoder{true};
		for (size_t i = 0; i < sizeof traffic_write; i++) {
			TEST_ASSERT_EQUAL_UINT(
				i + 1 == sizeof traffic_write ? kStatusSize : 0,
				receive(decoder, traffic_write, i));
		}
		assert_frame(large_response, kStatusSize, Command::status, expected);
		TEST_ASSERT_EQUAL_UINT(0, visits);
		TEST_ASSERT_EACH_EQUAL_UINT8(0x55, traffic_state, sizeof traffic_state);
	}
}

CASE(full_traffic_read,
	 "USB Traffic READ returns all 512 slots as one independent snapshot") {
	reset();
	std::memset(traffic_state, 0, sizeof traffic_state);
	traffic_state[0] = 1;
	traffic_state[traffic_bytes - 1] = 2;
	uint8_t request[] = {kVersion, 7, 0, static_cast<uint8_t>(Command::read),
						 0,        0, 2};
	crc_frame(request, sizeof request);
	Decoder decoder{true};
	for (size_t i = 0; i < sizeof request - 1; i++) {
		TEST_ASSERT_EQUAL_UINT(0, receive(decoder, request, i));
		TEST_ASSERT_EQUAL_UINT(0, visits);
	}
	TEST_ASSERT_EQUAL_UINT(61447,
						   receive(decoder, request, sizeof request - 1));
	assert_frame(large_response, 61447, Command::read, Status::ok);
	TEST_ASSERT_EQUAL_UINT(1, visits);
	TEST_ASSERT_EQUAL_MEMORY(traffic_state, large_response + kStatusSize,
							 sizeof traffic_state);
	std::memset(traffic_state, 0xff, sizeof traffic_state);
	TEST_ASSERT_EQUAL_UINT8(1, large_response[kStatusSize]);
	TEST_ASSERT_EACH_EQUAL_UINT8(0, large_response + kStatusSize + 1,
								 traffic_bytes - 2);
	TEST_ASSERT_EQUAL_UINT8(2, large_response[61446]);
}

CASE(closed_service_gate,
	 "USB disabled service frames never look up, visit or update component "
	 "State") {
	prepare_traffic_write();
	Decoder decoder;
	for (size_t i = 0; i < sizeof traffic_write; i++) {
		TEST_ASSERT_EQUAL_UINT(i + 1 == sizeof traffic_write ? kStatusSize : 0,
							   receive(decoder, traffic_write, i));
	}
	assert_frame(large_response, kStatusSize, Command::status,
				 Status::unknown_command);
	TEST_ASSERT_EACH_EQUAL_UINT8(0x55, traffic_state, sizeof traffic_state);
	for (auto command :
		 {Command::read, Command::update_traffic, Command::update_gnss}) {
		decoder.reset();
		uint8_t request[126]{};
		const size_t size = command == Command::read          ? 7
							: command == Command::update_gnss ? 70
															  : 126;
		request[0] = kVersion;
		request[1] = static_cast<uint8_t>(size);
		request[3] = static_cast<uint8_t>(command);
		request[6] = 2;
		crc_frame(request, size);
		for (size_t i = 0; i < size; i++) {
			TEST_ASSERT_EQUAL_UINT(i + 1 == size ? kStatusSize : 0,
								   receive(decoder, request, i));
		}
		assert_frame(large_response, kStatusSize, Command::status,
					 Status::unknown_command);
	}
	TEST_ASSERT_EQUAL_UINT(0, lookups);
	TEST_ASSERT_EQUAL_UINT(0, visits);
	TEST_ASSERT_EQUAL_UINT(0, updates);
}

int main() {
	UNITY_BEGIN();
	RUN_TEST(read);
	RUN_TEST(partial_write);
	RUN_TEST(write_bounds);
	RUN_TEST(write_shape);
	RUN_TEST(component_errors);
	RUN_TEST(capacity);
	RUN_TEST(traffic_batches);
	RUN_TEST(gnss_report);
	RUN_TEST(frame_limit);
	RUN_TEST(full_traffic_write);
	RUN_TEST(rejected_traffic_write);
	RUN_TEST(full_traffic_read);
	RUN_TEST(closed_service_gate);
	return UNITY_END();
}
#endif
