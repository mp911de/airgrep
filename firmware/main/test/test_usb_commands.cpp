#include <cstring>
#include <initializer_list>
#include <optional>

#include "display_settings.hpp"
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
constexpr size_t kGnssV1Size = 168;
uint8_t received_gnss[kGnssV1Size];
size_t lookups, visits, updates, target_count;
int64_t received_us;
bool accessible;
uint8_t received_traffic[120 * 32];

bool read_state(void* owner, std::span<uint8_t> out) {
	if (!accessible) {
		return false;
	}
	visits++;
	std::memcpy(out.data(), owner, out.size());
	return true;
}
bool write_state(void* owner, size_t offset, std::span<const uint8_t> bytes) {
	if (!accessible) {
		return false;
	}
	visits++;
	std::memcpy(static_cast<uint8_t*>(owner) + offset, bytes.data(),
				bytes.size());
	return true;
}
StateAccess find(void* owner, uint8_t component) {
	lookups++;
	if (component == 1) {
		return {sizeof state, owner, read_state, write_state};
	}
	if (component == 2) {
		return {65535, owner, read_state, write_state};
	}
	return {};
}

bool traffic(void*, const uint8_t* payload, size_t count, int64_t now_us) {
	if (!accessible || count > 32) {
		return false;
	}
	updates++;
	std::memcpy(received_traffic, payload, count * 120);
	target_count = count;
	received_us = now_us;
	return true;
}

bool gnss(void*, const uint8_t* payload, int64_t now_us) {
	if (!accessible) {
		return false;
	}
	std::memcpy(received_gnss, payload, sizeof received_gnss);
	received_us = now_us;
	updates++;
	return true;
}

const Services services{&state, find, 120, kGnssV1Size, traffic, gnss};

void reset() {
	std::memset(&state, 0x55, sizeof state);
	lookups = visits = updates = target_count = 0;
	received_us = 0;
	accessible = true;
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
}

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

CASE(bound_state, "USB State access stays bound to the owner that found it") {
	reset();
	State other{};
	Services second = services;
	second.owner = &other;
	const uint8_t request[] = {1};
	uint8_t output[kStatusSize + sizeof state];
	TEST_ASSERT_EQUAL_UINT(
		sizeof output, execute_data(Command::read, request, sizeof request, 0,
									second, output, sizeof output));
	TEST_ASSERT_EQUAL_MEMORY(&other, output + kStatusSize, sizeof other);
	TEST_ASSERT_EQUAL_UINT(
		sizeof output, execute_data(Command::read, request, sizeof request, 0,
									services, output, sizeof output));
	TEST_ASSERT_EQUAL_MEMORY(&state, output + kStatusSize, sizeof state);
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
	for (const size_t size : {size_t{0}, size_t{4}, size_t{5},
							  sizeof request - 2, sizeof request}) {
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

namespace {
std::optional<ListedTraffic> listing(void*, uint8_t* out, size_t limit) {
	const size_t count = limit < 10 ? limit : 10;
	for (size_t i = 0; i < count; i++) {
		std::memset(out + i * 5, static_cast<int>(i + 1), 5);
	}
	return ListedTraffic{10, count};
}
const Services listing_services{&state,  find, 120, kGnssV1Size,
								traffic, gnss, 5,   listing};
}

CASE(list_traffic,
	 "USB LIST_TRAFFIC returns how many Targets there are, how many follow "
	 "and those, cut to the limit and to the response") {
	constexpr size_t header = kStatusSize + 4;
	const struct {
		uint8_t limit;
		size_t room_bytes;
		uint8_t returned;
	} cases[] = {{3, 50, 3}, {200, 50, 10}, {200, 14, 2}, {0, 50, 0}};
	uint8_t response[header + 50 + 1];
	for (const auto& test : cases) {
		const uint8_t limit[] = {test.limit, 0};
		const size_t capacity = header + test.room_bytes;
		std::memset(response, 0xaa, sizeof response);
		const size_t size =
			execute_data(Command::list_traffic, limit, sizeof limit, 0,
						 listing_services, response, capacity);
		TEST_ASSERT_EQUAL_UINT(header + test.returned * 5, size);
		assert_frame(response, size, Command::list_traffic, Status::ok);
		const uint8_t counts[] = {10, 0, test.returned, 0};
		TEST_ASSERT_EQUAL_HEX8_ARRAY(counts, response + kStatusSize, 4);
		for (size_t i = 0; i < test.returned; i++) {
			TEST_ASSERT_EACH_EQUAL_UINT8(i + 1, response + header + i * 5, 5);
		}
		TEST_ASSERT_EACH_EQUAL_UINT8(0xaa, response + capacity,
									 sizeof response - capacity);
	}

	const uint8_t limit[] = {0, 0, 0};
	for (const size_t wrong : {size_t{0}, size_t{1}, size_t{3}}) {
		TEST_ASSERT_EQUAL_UINT(
			kStatusSize,
			execute_data(Command::list_traffic, limit, wrong, 0,
						 listing_services, response, sizeof response));
		assert_frame(response, kStatusSize, Command::status,
					 Status::invalid_length);
	}
	assert_status(Command::list_traffic, limit, 2, Status::internal_error);
}

namespace {
struct SettingsRequest {
	char key[kKeySize]{};
	display::Settings settings{};

	SettingsRequest() {
		static_assert(sizeof display::kSettingsKey <= sizeof key);
		std::memcpy(key, display::kSettingsKey, sizeof display::kSettingsKey);
	}
};
static_assert(sizeof(SettingsRequest) == kKeySize + sizeof(display::Settings));

struct SettingsBlob {
	std::optional<display::Settings> stored;
	bool writable = true;
};

constexpr Stored display_schema[] = {
	stored<display::Settings>(display::kSettingsKey)};

Services settings_services(SettingsBlob& blob) {
	Services result{};
	result.owner = &blob;
	result.settings = display_schema;
	result.load_settings = [](void* owner, const char* key, uint8_t* out,
							  size_t size) {
		const auto& settings = static_cast<SettingsBlob*>(owner)->stored;
		if (!settings || std::strcmp(key, display::kSettingsKey) ||
			size != sizeof(*settings)) {
			return false;
		}
		std::memcpy(out, &*settings, size);
		return true;
	};
	result.store_settings = [](void* owner, const char* key,
							   const uint8_t* data, size_t size) {
		auto& storage = *static_cast<SettingsBlob*>(owner);
		if (!storage.writable || std::strcmp(key, display::kSettingsKey) ||
			size != sizeof(display::Settings)) {
			return false;
		}
		std::memcpy(&storage.stored.emplace(), data, size);
		return true;
	};
	result.erase_settings = [](void* owner) {
		auto& storage = *static_cast<SettingsBlob*>(owner);
		if (!storage.writable) {
			return false;
		}
		storage.stored.reset();
		return true;
	};
	return result;
}

void expect_settings_status(const Services& device, Command command,
							const void* request, size_t size, Status expected) {
	uint8_t response[kStatusSize + sizeof(display::Settings)];
	const size_t length =
		execute_data(command, static_cast<const uint8_t*>(request), size, 0,
					 device, response, sizeof response);
	TEST_ASSERT_EQUAL_UINT(kStatusSize, length);
	assert_frame(response, length, Command::status, expected);
}

void expect_settings_read(const Services& device,
						  const display::Settings& expected) {
	SettingsRequest request;
	uint8_t response[kStatusSize + sizeof(display::Settings)];
	const size_t length = execute_data(
		Command::read_settings, reinterpret_cast<const uint8_t*>(&request),
		kKeySize, 0, device, response, sizeof response);
	TEST_ASSERT_EQUAL_UINT(sizeof response, length);
	assert_frame(response, length, Command::read_settings, Status::ok);
	TEST_ASSERT_EQUAL_MEMORY(&expected, response + kStatusSize,
							 sizeof expected);
}
}

CASE(settings, "USB display settings round-trip through the device schema") {
	SettingsBlob blob;
	const auto device = settings_services(blob);
	expect_settings_read(device, {});
	SettingsRequest request;
	request.settings.reserved_brightness = 42;
	request.settings.conflict_indicator = 1;
	expect_settings_status(device, Command::write_settings, &request,
						   sizeof request, Status::ok);
	TEST_ASSERT_TRUE(blob.stored.has_value());
	TEST_ASSERT_EQUAL_MEMORY(&request.settings, &*blob.stored,
							 sizeof request.settings);
	expect_settings_read(device, request.settings);
	expect_settings_status(device, Command::write_settings, nullptr, 0,
						   Status::ok);
	TEST_ASSERT_FALSE(blob.stored.has_value());
	expect_settings_read(device, {});
}

CASE(invalid_settings_write,
	 "Invalid sizes, versions and values preserve stored settings") {
	SettingsRequest request;
	display::Settings prior;
	prior.reserved_brightness = 73;
	SettingsBlob blob{prior};
	const auto device = settings_services(blob);
	uint8_t padded[sizeof request + 1]{};
	std::memcpy(padded, &request, sizeof request);
	for (const size_t size : {sizeof request - 1, sizeof request + 1}) {
		expect_settings_status(device, Command::write_settings, padded, size,
							   Status::unknown_settings);
		expect_settings_read(device, prior);
	}
	SettingsRequest wrong_version;
	wrong_version.settings.version++;
	SettingsRequest invalid_value;
	invalid_value.settings.conflict_indicator = 2;
	for (const auto& invalid : {wrong_version, invalid_value}) {
		expect_settings_status(device, Command::write_settings, &invalid,
							   sizeof invalid, Status::unknown_settings);
		expect_settings_read(device, prior);
	}
}

CASE(unknown_settings_key, "USB rejects unknown settings keys") {
	SettingsRequest request;
	std::strcpy(request.key, "other");
	SettingsBlob blob;
	const auto device = settings_services(blob);
	expect_settings_status(device, Command::read_settings, &request, kKeySize,
						   Status::unknown_settings);
	expect_settings_status(device, Command::write_settings, &request,
						   sizeof request, Status::unknown_settings);
}

CASE(unterminated_settings_key,
	 "USB requires a terminated fixed-width settings key") {
	SettingsRequest request;
	SettingsBlob blob;
	const auto device = settings_services(blob);
	request.key[kKeySize - 1] = 'x';
	expect_settings_status(device, Command::read_settings, &request, kKeySize,
						   Status::unknown_settings);
	std::memset(request.key, 'c', kKeySize);
	expect_settings_status(device, Command::read_settings, &request, kKeySize,
						   Status::unknown_settings);
}

CASE(malformed_settings_payload_length,
	 "USB rejects malformed settings payload lengths") {
	SettingsRequest request;
	SettingsBlob blob;
	const auto device = settings_services(blob);
	expect_settings_status(device, Command::read_settings, &request,
						   kKeySize - 1, Status::invalid_length);
	expect_settings_status(device, Command::read_settings, &request,
						   sizeof request, Status::invalid_length);
	expect_settings_status(device, Command::write_settings, &request, kKeySize,
						   Status::invalid_length);
}

CASE(invalid_stored_settings,
	 "An unsupported stored version reads as defaults") {
	display::Settings corrupt;
	corrupt.version++;
	corrupt.reserved_brightness = 42;
	SettingsBlob blob{corrupt};
	expect_settings_read(settings_services(blob), {});
}

CASE(settings_storage_failure,
	 "Failed settings writes and erases preserve prior storage") {
	display::Settings prior;
	prior.reserved_brightness = 42;
	SettingsBlob blob{prior, false};
	const auto device = settings_services(blob);
	SettingsRequest request;
	expect_settings_status(device, Command::write_settings, &request,
						   sizeof request, Status::internal_error);
	expect_settings_status(device, Command::write_settings, nullptr, 0,
						   Status::internal_error);
	expect_settings_read(device, prior);
}

CASE(no_settings_registry,
	 "A device with no settings registry rejects the display key") {
	const Services without_settings{};
	SettingsRequest request;
	expect_settings_status(without_settings, Command::read_settings, &request,
						   kKeySize, Status::unknown_settings);
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
	for (size_t i = 0; i < sizeof reports; i++) {
		reports[i] = static_cast<uint8_t>(i + 1);
	}
	for (const size_t count : {size_t{1}, size_t{32}}) {
		updates = 0;
		assert_status(Command::update_traffic, reports, count * 120,
					  Status::ok);
		TEST_ASSERT_EQUAL_UINT(1, updates);
		TEST_ASSERT_EQUAL_UINT(count, target_count);
		TEST_ASSERT_EQUAL_MEMORY(reports, received_traffic, count * 120);
		TEST_ASSERT_EQUAL_INT64(123456, received_us);
	}
	updates = 0;
	for (const size_t size :
		 {size_t{0}, size_t{119}, size_t{121}, sizeof reports}) {
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
	uint8_t report[kGnssV1Size + 1];
	for (size_t i = 0; i < sizeof report; ++i) {
		report[i] = static_cast<uint8_t>(i + 1);
	}
	for (const size_t size :
		 {size_t{0}, size_t{64}, kGnssV1Size - 1, kGnssV1Size + 1}) {
		assert_status(Command::update_gnss, report, size,
					  Status::invalid_length);
	}
	TEST_ASSERT_EQUAL_UINT(0, updates);
	assert_status(Command::update_gnss, report, kGnssV1Size, Status::ok);
	TEST_ASSERT_EQUAL_UINT(1, updates);
	TEST_ASSERT_EQUAL_MEMORY(report, received_gnss, kGnssV1Size);
	TEST_ASSERT_EQUAL_INT64(123456, received_us);
	accessible = false;
	assert_status(Command::update_gnss, report, kGnssV1Size,
				  Status::internal_error);
	TEST_ASSERT_EQUAL_UINT(1, updates);
}

CASE(frame_limit,
	 "USB executor rejects payloads beyond the wire length limit") {
	reset();
	const uint8_t byte = 1;
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

StateAccess find_traffic(void* owner, uint8_t component) {
	lookups++;
	return component == 2 ? StateAccess{sizeof traffic_state, owner, read_state,
										write_state}
						  : StateAccess{};
}

const Services traffic_services{traffic_state, find_traffic, 120,
								kGnssV1Size,   traffic,      gnss};

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
}

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
		uint8_t request[6 + kGnssV1Size]{};
		const size_t size = command == Command::read          ? 7
							: command == Command::update_gnss ? sizeof request
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
	RUN_TEST(bound_state);
	RUN_TEST(partial_write);
	RUN_TEST(write_bounds);
	RUN_TEST(write_shape);
	RUN_TEST(component_errors);
	RUN_TEST(list_traffic);
	RUN_TEST(settings);
	RUN_TEST(invalid_settings_write);
	RUN_TEST(unknown_settings_key);
	RUN_TEST(unterminated_settings_key);
	RUN_TEST(malformed_settings_payload_length);
	RUN_TEST(invalid_stored_settings);
	RUN_TEST(settings_storage_failure);
	RUN_TEST(no_settings_registry);
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
