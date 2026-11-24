#include <algorithm>

#include "check.hpp"
#include "device/device_state.hpp"

namespace {
// The fixture is the response to the next request. Opening a session drains old
// bytes.
class FixtureTransport : public device::Transport {
   public:
	std::vector<uint8_t> response;

	void reset_session() override { response.clear(); }
	void write(std::span<const uint8_t>) override {}
	bool read(std::vector<uint8_t>& bytes, size_t,
			  std::chrono::milliseconds) override {
		if (response.empty()) {
			return false;
		}
		bytes.insert(bytes.end(), response.begin(), response.end());
		response.clear();
		return true;
	}
};

void wire_layout() {
	using namespace usb::protocol;
	FixtureTransport transport;
	device::Session session(transport);
	const Hello sent{.uptime_seconds = 0x01020304,
					 .variant = "Variant",
					 .internal_free = 1300,
					 .internal_minimum = 1100,
					 .internal_total = 1500,
					 .psram_free = 2300,
					 .psram_minimum = 2100,
					 .psram_total = 2500,
					 .health = 0x4102,
					 .mode = 1,
					 .cpu_core0 = 17,
					 .cpu_core1 = 63,
					 .cpu_overall = 40,
					 .service = Service::present,
					 .firmware_description = "Firmware",
					 .board = "Board",
					 .chip = "Chip"};
	transport.response.resize(kHelloSize);
	encode_hello(sent, transport.response);
	const auto hello = session.hello();
	CHECK_EQUAL(0x01020304ULL, hello.uptime_seconds);
	CHECK_EQUAL(std::string("Variant"), hello.variant);
	CHECK_EQUAL(1300u, hello.internal_free);
	CHECK_EQUAL(1100u, hello.internal_minimum);
	CHECK_EQUAL(1500u, hello.internal_total);
	CHECK_EQUAL(2300u, hello.psram_free);
	CHECK_EQUAL(2100u, hello.psram_minimum);
	CHECK_EQUAL(2500u, hello.psram_total);
	CHECK_EQUAL(0x4102, hello.health);
	CHECK_EQUAL(1, hello.mode);
	CHECK_EQUAL(17, hello.cpu_core0);
	CHECK_EQUAL(63, hello.cpu_core1);
	CHECK_EQUAL(40, hello.cpu_overall);
	CHECK(hello.service == usb::protocol::Service::present);
	CHECK_EQUAL(std::string("Firmware"), hello.firmware_description);
	CHECK_EQUAL(std::string("Board"), hello.board);
	CHECK_EQUAL(std::string("Chip"), hello.chip);

	transport.response.resize(kStatusSize + 240);
	const auto payload = std::span(transport.response).subspan(kStatusSize);
	// V1 GNSS starts at byte 56. Coordinates are little-endian binary32.
	const uint8_t gnss[] = {2, 1, 11, 0, 0, 0, 0x41, 0x42, 0, 0, 0x38, 0xc1};
	std::ranges::copy(gnss, payload.begin() + 56);
	payload[232] = 1;
	encode_response(Command::read, usb::protocol::Status::ok,
					transport.response, payload.size());
	adsbee::StateData state;
	session.read(device::Component::adsbee, device::bytes_of(state));
	CHECK(state.gnss.fix == adsbee::Fix::fix_3d);
	CHECK(state.gnss.quality == adsbee::FixQuality::gps);
	CHECK_EQUAL(11, state.gnss.satellites_used);
	CHECK_EQUAL(48.25f, state.gnss.latitude);
	CHECK_EQUAL(-11.5f, state.gnss.longitude);
	CHECK(state.traffic_ingestion == adsbee::Ingestion::ignore);
	CHECK(state.gnss_ingestion == adsbee::Ingestion::read);
}

}

int main() { return check::run({TEST(wire_layout)}); }
