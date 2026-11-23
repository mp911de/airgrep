
#include <cmath>
#include <functional>
#include <string>

#include "800x480-touch/display_settings.hpp"
#include "check.hpp"
#include "device/dump.hpp"
#include "device/emulator.hpp"
#include "device/inspect.hpp"
#include "device/session.hpp"
#include "seeed-172x320-touch/display_settings.hpp"
#include "settings/settings_model.hpp"

namespace {
using device::Command;
using device::Component;
using device::DeviceState;
using device::Emulator;
using device::Session;
using namespace std::chrono_literals;

std::string failure(const std::function<void()>& action) {
	try {
		action();
	} catch (const device::Error& error) {
		return error.what();
	}
	return "";
}

bool mentions(const std::string& text, const char* word) {
	return text.find(word) != std::string::npos;
}

class FaultInjectingTransport : public device::Transport {
   public:
	explicit FaultInjectingTransport(Emulator& emulator)
		: emulator_(emulator) {}
	size_t piece = SIZE_MAX;
	size_t corrupt_byte_offset = SIZE_MAX;
	std::vector<uint8_t> appended;
	std::vector<uint8_t> replacement_response;
	int resets = 0;

	void reset_session() override {
		resets++;
		emulator_.reset_session();
	}
	void write(std::span<const uint8_t> frame) override {
		emulator_.write(frame);
		emulator_.read(pending_, SIZE_MAX, 0ms);
		if (corrupt_byte_offset < pending_.size()) {
			pending_[corrupt_byte_offset] ^= 0x10U;
		}
		if (!replacement_response.empty()) {
			pending_ = replacement_response;
		}
		pending_.insert(pending_.end(), appended.begin(), appended.end());
	}
	bool read(std::vector<uint8_t>& bytes, size_t,
			  std::chrono::milliseconds) override {
		const size_t count = std::min(piece, pending_.size());
		const auto end = pending_.begin() + static_cast<std::ptrdiff_t>(count);
		bytes.insert(bytes.end(), pending_.begin(), end);
		pending_.erase(pending_.begin(), end);
		return count > 0;
	}

   private:
	Emulator& emulator_;
	// Bytes already handed to the USB endpoint survive a session reset.
	std::vector<uint8_t> pending_;
};

void frames() {
	// The request vector of docs/usb-data-protocol.md.
	const std::vector<uint8_t> hello = {0x01, 0x06, 0x00, 0x01, 0xf5, 0x50};
	CHECK(device::request(Command::hello) == hello);
	usb::protocol::Decoder decoder(true);
	const uint8_t component = 4;
	const std::vector<uint8_t> read =
		device::request(Command::read, {&component, 1});
	bool complete = false;
	for (const uint8_t byte : read) {
		complete = decoder.feed(byte, 0);
	}
	CHECK(complete);
	CHECK(decoder.status() == usb::protocol::Status::ok);
	CHECK(decoder.command() == Command::read);
}

std::vector<uint8_t> zero_payload_response(Command command,
										   usb::protocol::Status status,
										   size_t payload_size) {
	std::vector<uint8_t> frame(usb::protocol::kStatusSize + payload_size);
	frame.resize(
		usb::protocol::encode_response(command, status, frame, payload_size));
	return frame;
}

void recompute_frame_crc(std::vector<uint8_t>& frame) {
	const uint16_t crc =
		usb::protocol::crc16(frame.data() + 6, frame.size() - 6,
							 usb::protocol::crc16(frame.data(), 4));
	frame[4] = static_cast<uint8_t>(crc);
	frame[5] = static_cast<uint8_t>(crc >> 8U);
}

unsigned read16(const std::vector<uint8_t>& bytes, size_t at) {
	return bytes[at] | static_cast<unsigned>(bytes[at + 1]) << 8U;
}

void responses() {
	using usb::protocol::Status;
	const auto refused = [](std::vector<uint8_t> frame, Command asked) {
		return failure([&] { device::payload_of(frame, asked); });
	};
	std::vector<uint8_t> hello = zero_payload_response(
		Command::hello, Status::ok, sizeof(device::Hello));
	CHECK_EQUAL(std::string(""), refused(hello, Command::hello));
	CHECK_EQUAL(sizeof(device::Hello),
				device::payload_of(hello, Command::hello).size());
	CHECK_EQUAL(std::string(""),
				refused(zero_payload_response(Command::status, Status::ok, 0),
						Command::reboot));

	CHECK(mentions(refused({1, 7, 0}, Command::hello),
				   "Invalid response length"));
	std::vector<uint8_t> cut = hello;
	cut.pop_back();
	CHECK(mentions(refused(cut, Command::hello), "Invalid response length"));
	CHECK(
		mentions(refused(zero_payload_response(Command::status, Status::ok, 1),
						 Command::reboot),
				 "unexpected payload"));
	CHECK(mentions(refused(zero_payload_response(Command::status,
												 Status::invalid_range, 0),
						   Command::write),
				   "INVALID_RANGE"));
	std::vector<uint8_t> newer = hello;
	newer[0] = 2;
	recompute_frame_crc(newer);
	CHECK(mentions(refused(newer, Command::hello), "version mismatch"));
	CHECK(mentions(refused(hello, Command::read), "Unexpected response"));
	CHECK(
		mentions(refused(zero_payload_response(Command::status, Status::ok, 0),
						 Command::hello),
				 "Unexpected response"));
	CHECK(mentions(refused(hello, Command::reboot), "Unexpected response"));
}

void conversation() {
	device::EmulatedDevice device = device::make_booted_service_device();
	device::fly(device, 12);
	Emulator emulator(device);
	Session session(emulator);

	device::Hello hello = session.hello();
	CHECK_EQUAL(std::string("0.0.0-emulated"), hello.firmware_version);
	CHECK_EQUAL(std::string("Emulated"), hello.board);
	CHECK_EQUAL(612u, hello.uptime_seconds);
	CHECK(hello.service == usb::protocol::Service::present);

	const auto snapshot =
		session.read_device_state(device::kDisplayComponents, 10);
	auto gauge = snapshot.states.battery;
	CHECK(gauge.soc > 77.9f && gauge.soc < 78.0f);
	CHECK_EQUAL(1, gauge.charge_duration_hours.valid);
	const auto link = snapshot.states.adsbee;
	CHECK(link.gnss.fix == adsbee::Fix::fix_3d);
	CHECK_EQUAL(11, link.gnss.satellites_used);
	CHECK_EQUAL(device.states.adsbee.gnss.latitude, link.gnss.latitude);
	std::vector<traffic::Target> targets(usb::layout::kTargets);
	session.read(Component::traffic,
				 {reinterpret_cast<uint8_t*>(targets.data()),
				  targets.size() * sizeof(traffic::Target)});
	CHECK_EQUAL(std::string("DEFGH"), targets[0].callsign);
	CHECK_EQUAL(0, targets[10].last_seen_us);
	for (uint8_t id = 1; id <= 9; id++) {
		auto component = static_cast<Component>(id);
		const std::span<uint8_t> held = device.states.bytes(component);
		CHECK(session.read(component) ==
			  std::vector<uint8_t>(held.begin(), held.end()));
	}

	CHECK(mentions(failure([&] {
					   session.read(Component::charger,
									device::bytes_of(gauge));
				   }),
				   "charger has 288 bytes on the device and 72 here"));
	CHECK(mentions(failure([&] { session.read(static_cast<Component>(42)); }),
				   "UNKNOWN_COMPONENT"));

	session.reboot();
	CHECK(emulator.restart_command == Command::reboot);
	session.enter_bootloader();
	CHECK(emulator.restart_command == Command::enter_bootloader);
}

void listing() {
	device::EmulatedDevice device = device::make_booted_service_device();
	device::fly(device, traffic::kHistorySeconds + 10);
	Emulator emulator(device);
	Session session(emulator);
	auto listing = session.list_traffic(3);
	auto& listed = listing.nearest_targets;
	CHECK_EQUAL(10, listing.total_targets);
	CHECK_EQUAL(3u, listed.size());
	const adsbee::Gnss& own = device.states.adsbee.gnss;
	float last = 0;
	for (const traffic::Tracked& tracked : listed) {
		float distance = NAN, bearing = NAN;
		traffic::relative_position(tracked.target, own.latitude, own.longitude,
								   distance, bearing);
		CHECK(distance >= last);
		last = distance;
		for (const traffic::Position& second : tracked.past) {
			CHECK(second.sample == traffic::Sample::fresh);
		}
		CHECK(tracked.past[1].latitude != tracked.past[0].latitude ||
			  tracked.past[1].longitude != tracked.past[0].longitude);
	}
	listing = session.list_traffic(64);
	CHECK_EQUAL(10, listing.total_targets);
	CHECK_EQUAL(10u, listed.size());
	listing = session.list_traffic(0);
	CHECK_EQUAL(10, listing.total_targets);
	CHECK_EQUAL(0u, listed.size());

	device::fly(device, 3);
	listing = session.list_traffic(1);
	CHECK(listed[0].past[3].sample == traffic::Sample::fresh);
	CHECK(listed[0].past[4].sample == traffic::Sample::none);

	device.states.traffic.clear();
	CHECK(
		mentions(failure([&] { session.list_traffic(3); }), "INTERNAL_ERROR"));
	device.states.traffic.resize(10);
	listing = session.list_traffic(3);
	CHECK_EQUAL(0, listing.total_targets);
	CHECK(listed.empty());

	device::EmulatedDevice plain = device::make_booted_service_device();
	plain.hello.service = usb::protocol::Service::absent;
	Emulator closed(plain);
	Session refused(closed);
	CHECK(
		mentions(failure([&] { refused.list_traffic(3); }), "UNKNOWN_COMMAND"));
}

void ingestion_override_ends_at_session_reset() {
	device::EmulatedDevice device = device::make_booted_service_device();
	Emulator emulator(device);
	FaultInjectingTransport transport(emulator);
	Session session(transport);
	CHECK_EQUAL(1, transport.resets);
	constexpr uint8_t ingestion_wire_offset = 232;
	std::vector<uint8_t> write = {1, ingestion_wire_offset, 0, 1, 0};
	const std::span<uint8_t> state = device.states.bytes(Component::adsbee);
	write.insert(write.end(), state.begin(), state.end());
	write[5 + ingestion_wire_offset] = 1;
	session.send(Command::write, write);
	CHECK(session.read_device_state(device::kDisplayComponents, 1)
			  .states.adsbee.traffic_ingestion == adsbee::Ingestion::ignore);
	CHECK_EQUAL(1, transport.resets);
	transport.write(device::request(Command::hello));
	Session next(transport);
	CHECK(next.read_device_state(device::kDisplayComponents, 1)
			  .states.adsbee.traffic_ingestion == adsbee::Ingestion::read);
}

void refusals() {
	device::EmulatedDevice plain = device::make_booted_service_device();
	plain.hello.service = usb::protocol::Service::absent;
	Emulator emulator(plain);
	Session session(emulator);
	CHECK(session.hello().service == usb::protocol::Service::absent);
	CHECK(mentions(failure([&] { session.read(Component::battery); }),
				   "UNKNOWN_COMMAND"));
}

usb::protocol::Status refusal(const std::function<void()>& action) {
	try {
		action();
	} catch (const device::Refused& refused) {
		return refused.status;
	}
	return usb::protocol::Status::ok;
}

void stored_settings() {
	using usb::protocol::Status;
	using Bytes = std::vector<uint8_t>;
	device::EmulatedDevice device = device::make_booted_service_device();
	Emulator emulator(device);
	Session session(emulator);

	// The frames as docs/usb-data-protocol.md lays them out: the key in 16
	// bytes, then the struct.
	Bytes read = device::request(Command::read_settings,
								 Bytes{'8', '0', '0', 'x', '4', '8', '0', '-',
									   't', 'o', 'u', 'c', 'h', 0, 0, 0});
	CHECK_EQUAL(22u, read.size());
	CHECK_EQUAL(0x09, read[3]);

	const Bytes documented_v1_defaults = {1,   0, 0, 'e', 'n', 0,
										  255, 0, 1, 1,   0,   0};
	CHECK(session.read_settings("800x480-touch") == documented_v1_defaults);
	CHECK(session.read_settings("172x320-touch") == documented_v1_defaults);
	CHECK(device.flash->values.empty());

	enum : size_t {
		theme_byte = 2,
		language_bytes = 3,
		zoom_byte = 8,
		symbol_byte = 11
	};
	Bytes chosen = documented_v1_defaults;
	chosen[theme_byte] = 1;
	chosen[language_bytes] = 'd';
	chosen[language_bytes + 1] = 'e';
	chosen[zoom_byte] = 3;
	chosen[symbol_byte] = 1;
	session.write_settings("800x480-touch", chosen);
	CHECK(session.read_settings("800x480-touch") == chosen);
	CHECK(session.read_settings("172x320-touch") == documented_v1_defaults);
	display::touch_800x480::Settings stored;
	CHECK_EQUAL(sizeof stored, chosen.size());
	std::memcpy(&stored, chosen.data(), sizeof stored);
	CHECK(stored.theme == display::touch_800x480::Theme::dark);
	CHECK_EQUAL(std::string("de"), stored.language);
	CHECK_EQUAL(16, display::RANGES_NM[stored.radar_range_index]);
	CHECK(stored.symbol_style == display::touch_800x480::SymbolStyle::simple);

	const Bytes shorter(chosen.begin(), chosen.end() - 1);
	Bytes longer = chosen;
	longer.push_back(0);
	Bytes newer = chosen, zoomed = chosen, spoken = chosen, styled = chosen;
	newer[0] = 2;
	zoomed[zoom_byte] = 5;
	spoken[language_bytes] = 'D';
	styled[symbol_byte] = 2;
	for (const Bytes& wrong :
		 {shorter, longer, newer, zoomed, spoken, styled}) {
		CHECK(refusal([&] {
				  session.write_settings("800x480-touch", wrong);
			  }) == Status::unknown_settings);
	}
	CHECK(session.read_settings("800x480-touch") == chosen);
	CHECK_EQUAL(std::string("Emulated"), session.hello().board);

	CHECK(refusal([&] { session.read_settings("800x480"); }) ==
		  Status::unknown_settings);
	CHECK(refusal([&] { session.write_settings("seeed-touch", chosen); }) ==
		  Status::unknown_settings);
	CHECK(mentions(failure([&] { session.read_settings("0123456789abcdef"); }),
				   "too long"));

	Bytes general = session.read_settings(SETTINGS_NVS_KEY);
	settings::Settings own = settings::defaults();
	CHECK_EQUAL(sizeof own, general.size());
	CHECK(std::memcmp(general.data(), &own, sizeof own) == 0);
	CHECK_EQUAL(5000U, read16(general, 70));
	own.altitude_filter_ft = 1500;
	const auto* bytes = reinterpret_cast<const uint8_t*>(&own);
	session.write_settings(SETTINGS_NVS_KEY, {bytes, sizeof own});
	general = session.read_settings(SETTINGS_NVS_KEY);
	CHECK_EQUAL(1500U, read16(general, 86));
	own.altitude_filter_ft = 10000;
	CHECK(refusal([&] {
			  session.write_settings(SETTINGS_NVS_KEY, {bytes, sizeof own});
		  }) == Status::unknown_settings);

	session.erase_settings();
	CHECK(device.flash->values.empty());
	CHECK(session.read_settings("800x480-touch") == documented_v1_defaults);
	general = session.read_settings(SETTINGS_NVS_KEY);
	CHECK_EQUAL(3000U, read16(general, 86));

	device::EmulatedDevice plain = device::make_booted_service_device();
	plain.hello.service = usb::protocol::Service::absent;
	Emulator closed(plain);
	Session other(closed);
	CHECK(refusal([&] { other.read_settings("800x480-touch"); }) ==
		  Status::unknown_command);
	CHECK(refusal([&] { other.erase_settings(); }) == Status::unknown_command);
}

void damage() {
	device::EmulatedDevice device = device::make_booted_service_device();
	Emulator emulator(device);
	FaultInjectingTransport transport(emulator);
	Session session(transport);

	transport.piece = 1;
	CHECK_EQUAL(std::string("Emulated"), session.hello().board);
	CHECK_EQUAL(sizeof(wifi::StateData), session.read(Component::wifi).size());
	transport.piece = SIZE_MAX;

	transport.corrupt_byte_offset = 20;
	CHECK(mentions(failure([&] { session.hello(); }), "CRC mismatch"));
	transport.corrupt_byte_offset = SIZE_MAX;

	const std::vector<uint8_t> frame_with_length_below_header = {1, 3, 0, 0x80,
																 0, 0, 0};
	transport.replacement_response = frame_with_length_below_header;
	CHECK(
		mentions(failure([&] { session.hello(); }), "smaller than its header"));
	transport.replacement_response.clear();

	transport.appended = {0};
	CHECK(mentions(failure([&] { session.hello(); }), "Unexpected bytes"));
	transport.appended.clear();

	transport.piece = 0;
	CHECK(mentions(failure([&] { session.hello(); }), "timed out"));
}

// What a read of every Component of the device returns.
DeviceState state_of(const device::EmulatedDevice& device) {
	return {.hello = device.hello,
			.components = {std::begin(device::kComponents),
						   std::end(device::kComponents)},
			.states = device.states};
}

void inspection() {
	constexpr int64_t now_us = 700'000'000;
	device::EmulatedDevice device = device::make_booted_service_device();
	device::fly(device, 60);
	device.states.traffic[3].squawk = 7000;
	device.states.barometer.osr.temperature = 128;
	device.states.barometer.iir.pressure = 127;
	const device::Inspection clean = device::inspect(state_of(device), now_us);
	CHECK_EQUAL(0, clean.problems);
	CHECK(clean.fields > 600);
	CHECK(mentions(clean.text, "  battery.soc = 77.8"));
	CHECK(mentions(clean.text, "  traffic.targets = 10"));

	device.states.battery.charge_duration_hours.valid = 2;
	CHECK(
		mentions(device::inspect(state_of(device), now_us).first_invalid_field,
				 "charge_duration_hours.valid"));
	device.states.battery.charge_duration_hours.valid = 1;

	device.states.bytes(
		Component::expander)[offsetof(expander::StateData, input)] = 2;
	device.states.adsbee.last_gnss_report_us = now_us + 1;
	device.states.traffic[3].position_valid = true;
	device.states.traffic[3].latitude = 123;
	device.states.traffic[3].squawk = 1800;
	device.states.barometer.osr.pressure = 3;
	device.states.barometer.iir.pressure = 8;
	std::memset(device.states.wifi.last_reported_config.ssid, 'x',
				sizeof device.states.wifi.last_reported_config.ssid);
	device.hello.mode = 9;
	const device::Inspection found = device::inspect(state_of(device), now_us);
	CHECK_EQUAL(8, found.problems);
	CHECK(mentions(found.text, "!!traffic[3].squawk"));
	CHECK(mentions(found.text, "!!barometer.osr.pressure"));
	CHECK(mentions(found.text, "!!barometer.iir.pressure"));
	CHECK(mentions(found.text, "!!expander.input = ?0000000011   <-- a bool"));
	CHECK(mentions(found.text, "!!adsbee.gnss_us = 700000001   <-- ahead"));
	CHECK(mentions(found.text, "!!traffic[3].latitude = 123   <-- outside"));
	CHECK(mentions(found.text, "!!wifi.reported.ssid"));
	CHECK(mentions(found.text, "!!hello.mode = 9   <-- outside 0 to 3"));
}
void dumping() {
	DeviceState picture{.components = {std::begin(device::kDisplayComponents),
									   std::end(device::kDisplayComponents)}};
	picture.hello.uptime_seconds = 42;
	picture.states.adsbee.gnss.fix = adsbee::Fix::fix_3d;
	picture.states.adsbee.gnss.latitude = 50;
	picture.states.adsbee.gnss.longitude = 8;
	traffic::Tracked tracked{};
	std::strcpy(tracked.target.callsign, "RYR5TE");
	tracked.target.address = 0x4CA7E3;
	tracked.target.last_seen_us = 41'500'000;
	tracked.target.position_valid = tracked.target.baro_alt_valid =
		tracked.target.speed_valid = true;
	tracked.target.latitude = 50 + 10 / 60.0f;
	tracked.target.longitude = 8;
	tracked.target.baro_alt_ft = 38000;
	tracked.target.speed_kt = 450;
	tracked.past[0] = {tracked.target.latitude,
					   8,
					   38000,
					   450,
					   traffic::Sample::fresh,
					   traffic::AltitudeDatum::barometric};
	tracked.past[1] = {50.16f,
					   8,
					   38000,
					   450,
					   traffic::Sample::held,
					   traffic::AltitudeDatum::unknown};
	tracked.past[2] = {50.15f,
					   8,
					   0,
					   450,
					   traffic::Sample::fresh,
					   traffic::AltitudeDatum::unavailable};
	picture.traffic_listing.nearest_targets.push_back(tracked);
	picture.traffic_listing.total_targets = 1;

	const std::string text = device::dump(picture);
	CHECK(mentions(text, "== Device, 42 s after its start"));
	CHECK(mentions(text, "fix 3D  position 50.00000 8.00000"));
	CHECK(mentions(text, "== Traffic, 1 listed of 1 the device holds"));
	CHECK(mentions(text, "RYR5TE   4CA7E3 1090 ADS-B  seen at 41.5 s"));
	CHECK(mentions(text, "  from own ship 10.01 NM at 000.0 true"));
	CHECK(mentions(text, "  altitude baro 38000 ft gnss -  speed 450 kt"));
	CHECK(mentions(text, "  -00 s  50.16667 8.00000  38000 ft  450 kt  fresh"));
	CHECK(mentions(text, "  -01 s  50.16000 8.00000  38000 ft  450 kt  held"));
	CHECK(mentions(text, "  -02 s  50.15000 8.00000  -  450 kt  fresh"));
	CHECK(!mentions(text, "  -03 s"));
	CHECK(mentions(text, "  listed[0].baro_alt_ft = 38000"));
	CHECK(mentions(text, "barometer, "));
	CHECK(mentions(text, "listed 4CA7E3, "));
	CHECK(mentions(text, "  0000  e3 a7 4c 00"));

	picture.states.adsbee.gnss.fix = adsbee::Fix::none;
	CHECK(mentions(device::dump(picture), "  from own ship unknown"));
}
}

int main() {
	return check::run({
		TEST(inspection),
		TEST(dumping),
		TEST(frames),
		TEST(responses),
		TEST(conversation),
		TEST(listing),
		TEST(ingestion_override_ends_at_session_reset),
		TEST(refusals),
		TEST(stored_settings),
		TEST(damage),
	});
}
