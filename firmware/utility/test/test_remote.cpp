#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

#include "check.hpp"
#include "device/emulator.hpp"
#include "remote/connection.hpp"

namespace {
using device::Command;
using remote::Connection;
using remote::Conversation;
using namespace std::chrono_literals;

template <typename Reached>
bool eventually(Reached reached) {
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (!reached()) {
		if (std::chrono::steady_clock::now() >= deadline) {
			return false;
		}
		std::this_thread::sleep_for(1ms);
	}
	return true;
}

struct ConnectionFixture {
	device::EmulatedDevice device = device::make_booted_service_device();
	device::Emulator emulator;
	std::atomic<bool> gone = false;

	explicit ConnectionFixture(
		bool service_commands = true,
		usb::protocol::Service advertised = usb::protocol::Service::present)
		: emulator(device, service_commands) {
		device.hello.service = advertised;
	}

	std::vector<Command> requests() {
		const std::scoped_lock lock(mutex);
		return log;
	}
	void pause_at(Command command) {
		const std::scoped_lock lock(mutex);
		gate = command;
	}
	bool wait_for_pause() {
		std::unique_lock lock(mutex);
		return wake.wait_for(lock, 2s, [&] { return paused; });
	}
	void resume() {
		const std::scoped_lock lock(mutex);
		gate.reset();
		wake.notify_all();
	}

   private:
	void record(Command command) {
		std::unique_lock lock(mutex);
		log.push_back(command);
		if (gate == command) {
			paused = true;
			wake.notify_all();
			if (!wake.wait_for(lock, 2s, [&] { return !gate; })) {
				throw device::Error("Test did not release the request");
			}
			paused = false;
		}
	}
	std::mutex mutex;
	std::condition_variable wake;
	std::vector<Command> log;
	std::optional<Command> gate;
	bool paused = false;

   public:
	class Wire : public device::Transport {
	   public:
		explicit Wire(ConnectionFixture& bench) : bench_(bench) {}
		void reset_session() override { bench_.emulator.reset_session(); }
		void write(std::span<const uint8_t> frame) override {
			bench_.record(static_cast<Command>(frame[3]));
			if (bench_.gone) {
				throw device::Error("Device does not answer: No such device");
			}
			bench_.emulator.write(frame);
		}
		bool read(std::vector<uint8_t>& bytes, size_t wanted,
				  std::chrono::milliseconds timeout) override {
			return bench_.emulator.read(bytes, wanted, timeout);
		}

	   private:
		ConnectionFixture& bench_;
	};

	Wire wire{*this};
	Connection::Open open() {
		return [this] { return std::make_unique<Wire>(*this); };
	}
};

// A Conversation begins at this time. The tests choose every later one.
constexpr Conversation::Time kStart{};
std::vector<uint8_t> default_settings() {
	return {1, 0, 0, 'e', 'n', 0, 255, 0, 1, 1, 0, 0};
}

bool connected(const Connection& connection) {
	return connection.status().phase == Connection::Phase::connected;
}

bool ended(const Connection& connection) {
	return connection.status().phase == Connection::Phase::ended;
}

template <typename Action>
std::string failure(Action action) {
	try {
		action();
	} catch (const device::Error& error) {
		return error.what();
	}
	return "";
}

void idle_conversation_says_hello_every_second() {
	ConnectionFixture bench;
	Conversation conversation(bench.wire);
	CHECK(conversation.supports_component_reads());
	CHECK_EQUAL(std::string("Emulated"), conversation.hello().board);
	const size_t opening = bench.requests().size();

	Conversation::Step step = conversation.step(kStart, {});
	CHECK(step.next == kStart + 1s);
	CHECK(!step.picture);
	CHECK_EQUAL(opening + 1, bench.requests().size());
	step = conversation.step(kStart + 999ms, {});
	CHECK(step.next == kStart + 1s);
	CHECK_EQUAL(opening + 1, bench.requests().size());
	step = conversation.step(kStart + 1s, {});
	CHECK(step.next == kStart + 2s);
	CHECK_EQUAL(opening + 2, bench.requests().size());
	CHECK(std::ranges::all_of(bench.requests(), [](Command command) {
		return command == Command::hello;
	}));
}

void picture_holds_what_a_display_shows() {
	ConnectionFixture bench;
	device::fly(bench.device, 5);
	Conversation conversation(bench.wire);
	const Conversation::Step step =
		conversation.step(kStart, {.interval = 5ms});
	CHECK(step.picture.has_value());
	if (!step.picture) {
		return;
	}
	const device::DeviceState& picture = *step.picture;
	CHECK(picture.states.adsbee.gnss.fix == adsbee::Fix::fix_3d);
	CHECK_EQUAL(10, picture.traffic_listing.total_targets);
	CHECK_EQUAL(10u, picture.traffic_listing.nearest_targets.size());
	CHECK_EQUAL(std::string("DEFGH"),
				picture.traffic_listing.nearest_targets[0].target.callsign);
	CHECK(picture.traffic_listing.nearest_targets[0].past[0].sample ==
		  traffic::Sample::fresh);
	CHECK(picture.states.battery.soc > 77);
	CHECK_EQUAL(605u, picture.hello.uptime_seconds);
	CHECK_EQUAL(605u, conversation.hello().uptime_seconds);
}

void pictures_follow_the_watch_interval() {
	ConnectionFixture bench;
	Conversation conversation(bench.wire);
	const Conversation::Asked watching{.interval = 200ms};
	Conversation::Step step = conversation.step(kStart, watching);
	CHECK(step.picture.has_value());
	CHECK(step.next == kStart + 200ms);
	step = conversation.step(kStart + 199ms, watching);
	CHECK(!step.picture);
	CHECK(step.next == kStart + 200ms);
	step = conversation.step(kStart + 200ms, watching);
	CHECK(step.picture.has_value());
	CHECK(step.next == kStart + 400ms);
	// A picture carries HELLO, so the next one is due a second after it.
	step = conversation.step(kStart + 400ms, {});
	CHECK(!step.picture);
	CHECK(step.next == kStart + 1200ms);
}

void device_takes_the_gnss_data_it_is_handed() {
	ConnectionFixture bench;
	Conversation conversation(bench.wire);
	Conversation::Step step = conversation.step(kStart, {.interval = 200ms});
	adsbee::Gnss handed = bench.device.states.adsbee.gnss;
	handed.fix = adsbee::Fix::fix_2d;
	handed.satellites_used = 4;
	// Handed over before the picture of that step is read.
	step =
		conversation.step(kStart + 200ms, {.interval = 200ms, .gnss = handed});
	CHECK(step.picture.has_value());
	if (step.picture) {
		const adsbee::Gnss& read = step.picture->states.adsbee.gnss;
		CHECK(read.fix == adsbee::Fix::fix_2d);
		CHECK_EQUAL(4, read.satellites_used);
	}
	CHECK(bench.device.states.adsbee.gnss.fix == adsbee::Fix::fix_2d);
}

void changed_watch_interval_reschedules_the_next_picture() {
	ConnectionFixture bench;
	Conversation conversation(bench.wire);
	Conversation::Step step = conversation.step(kStart, {.interval = 10s});
	CHECK(step.picture.has_value());
	CHECK(step.next == kStart + 1s);
	step = conversation.step(kStart + 5ms, {.interval = 5ms});
	CHECK(step.picture.has_value());
	CHECK(step.next == kStart + 10ms);
}

void unreadable_data_ends_the_conversation() {
	{
		ConnectionFixture bench;
		bench.device.states.bytes(
			device::Component::expander)[offsetof(expander::StateData, input)] =
			7;
		Conversation conversation(bench.wire);
		CHECK(failure([&] {
				  conversation.step(kStart, {.interval = 5ms});
			  }).find("cannot read: expander.input") != std::string::npos);
	}
	{
		ConnectionFixture bench;
		std::memset(bench.device.hello.board, 'x',
					sizeof bench.device.hello.board);
		CHECK(failure([&] {
				  const Conversation conversation(bench.wire);
			  }).find("hello.board") != std::string::npos);
	}
}

void hello_notices_a_device_that_is_gone() {
	ConnectionFixture bench;
	Conversation conversation(bench.wire);
	bench.gone = true;
	CHECK_EQUAL(std::string("Device does not answer: No such device"),
				failure([&] { conversation.step(kStart, {}); }));
	CHECK(bench.requests().back() == Command::hello);
}

void restart_ends_the_conversation() {
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		CHECK(!conversation
				   .step(kStart, {.restart = Conversation::Restart::reboot})
				   .next);
		CHECK(bench.emulator.restart_command == Command::reboot);
	}
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		CHECK(!conversation
				   .step(kStart, {.restart = Conversation::Restart::bootloader})
				   .next);
		CHECK(bench.emulator.restart_command == Command::enter_bootloader);
	}
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		CHECK(!conversation.step(kStart, {.stop = true}).next);
		CHECK(!bench.emulator.restart_command.has_value());
	}
}

void device_without_service_commands_gives_no_picture() {
	{
		ConnectionFixture bench(false, usb::protocol::Service::absent);
		Conversation conversation(bench.wire);
		CHECK(!conversation.supports_component_reads());
		const Conversation::Step step =
			conversation.step(kStart, {.interval = 5ms});
		CHECK(!step.picture);
		CHECK(step.next == kStart + 1s);
	}
	{
		// Firmware older than the field shows only by its answer to a READ
		// whether it knows one.
		ConnectionFixture bench(true, usb::protocol::Service::unknown);
		CHECK(Conversation(bench.wire).supports_component_reads());
	}
	{
		ConnectionFixture bench(false, usb::protocol::Service::unknown);
		CHECK(!Conversation(bench.wire).supports_component_reads());
	}
}

void settings() {
	const std::string key = "800x480-touch";
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		Conversation::Step step =
			conversation.step(kStart, {.settings_key = key});
		CHECK(step.settings == default_settings());
		// Nothing waits for the settings: the next request follows at once.
		CHECK(step.next == kStart);

		std::vector<uint8_t> zoomed = default_settings();
		zoomed[2] = 1;
		zoomed[8] = 4;
		step = conversation.step(
			kStart, {.settings_key = key, .settings_write = {{key, zoomed}}});
		CHECK(step.settings == zoomed);

		step = conversation.step(kStart, {.settings_key = "1024x600-touch"});
		CHECK(step.settings && step.settings->empty());

		CHECK(!conversation
				   .step(kStart,
						 {.restart = Conversation::Restart::factory_reset})
				   .next);
		CHECK(bench.emulator.restart_command == Command::reboot);
		CHECK(bench.device.flash->values.empty());
	}
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		CHECK_EQUAL(std::string("Device replied UNKNOWN"), failure([&] {
						conversation.step(kStart,
										  {.settings_write = {{key, {1, 0}}}});
					}));
	}
	{
		ConnectionFixture bench(false, usb::protocol::Service::absent);
		Conversation conversation(bench.wire);
		const Conversation::Step step =
			conversation.step(kStart, {.settings_key = key});
		CHECK(step.settings && step.settings->empty());
		CHECK_EQUAL(std::string("Device replied UNKNOWN_COMMAND"), failure([&] {
						conversation.step(
							kStart,
							{.restart = Conversation::Restart::factory_reset});
					}));
		CHECK(!bench.emulator.restart_command.has_value());
	}
}

void settings_are_written_before_a_restart_and_before_the_end() {
	const std::vector<uint8_t> dark = {1,   0, 1, 'e', 'n', 0,
									   255, 0, 1, 1,   0,   0};
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		CHECK(!conversation
				   .step(kStart, {.restart = Conversation::Restart::reboot,
								  .settings_write = {{"800x480-touch", dark}}})
				   .next);
		const std::vector<Command> requests = bench.requests();
		CHECK(requests.size() >= 2 &&
			  requests[requests.size() - 2] == Command::write_settings &&
			  requests.back() == Command::reboot);
		CHECK(bench.device.flash->values["800x480-touch"] == dark);
	}
	{
		ConnectionFixture bench;
		Conversation conversation(bench.wire);
		CHECK(!conversation
				   .step(kStart, {.settings_write = {{"800x480-touch", dark}},
								  .stop = true})
				   .next);
		CHECK(bench.device.flash->values["800x480-touch"] == dark);
	}
}

// From here on the Connection: its thread, and what is asked of it while a
// request is on its way.
void connection_shows_pictures_until_it_is_destroyed() {
	ConnectionFixture bench;
	device::fly(bench.device, 5);
	device::DeviceState picture;
	{
		Connection connection(bench.open());
		CHECK(eventually([&] { return connected(connection); }));
		CHECK_EQUAL(std::string("Emulated"), connection.status().hello.board);
		CHECK(!connection.take_new_picture(picture));
		CHECK(!connection.copy_latest_picture(picture));
		CHECK_EQUAL(0u, connection.status().pictures);

		connection.watch(5ms);
		CHECK(eventually([&] { return connection.take_new_picture(picture); }));
		device::DeviceState again;
		CHECK(connection.copy_latest_picture(again));
		CHECK_EQUAL(10u, again.traffic_listing.nearest_targets.size());
		CHECK_EQUAL(605u, picture.hello.uptime_seconds);
	}
	CHECK(bench.requests().back() != Command::reboot);
}

void picture_read_after_the_watch_ended_is_dropped() {
	ConnectionFixture bench;
	bench.pause_at(Command::list_traffic);
	Connection connection(bench.open());
	connection.watch(5ms);
	CHECK(bench.wait_for_pause());
	connection.watch(0ms);
	bench.resume();
	connection.read_settings("800x480-touch");
	CHECK(eventually([&] { return connection.settings_read().has_value(); }));
	device::DeviceState picture;
	CHECK(!connection.take_new_picture(picture));
	CHECK(!connection.copy_latest_picture(picture));
	CHECK_EQUAL(0u, connection.status().pictures);
}

void newer_settings_replace_those_that_wait() {
	ConnectionFixture bench;
	Connection connection(bench.open());
	CHECK(!connection.settings_read());
	bench.pause_at(Command::read_settings);
	connection.read_settings("800x480-touch");
	CHECK(bench.wait_for_pause());
	std::vector<uint8_t> dark = default_settings(), zoomed = default_settings();
	dark[2] = 1;
	zoomed[2] = 1;
	zoomed[8] = 4;
	connection.write_settings("800x480-touch", dark);
	connection.write_settings("800x480-touch", zoomed);
	connection.read_settings("800x480-touch");
	bench.resume();
	std::optional<std::vector<uint8_t>> read;
	CHECK(eventually([&] { return bool(read = connection.settings_read()); }));
	CHECK(read == zoomed);
	CHECK(!connection.settings_read());
	CHECK_EQUAL(1,
				std::ranges::count(bench.requests(), Command::write_settings));
}

void connection_ends_with_the_reason() {
	{
		ConnectionFixture bench;
		Connection connection(bench.open());
		connection.watch(5ms);
		CHECK(eventually([&] { return connection.status().pictures > 0; }));
		bench.gone = true;
		CHECK(eventually([&] { return ended(connection); }));
		CHECK_EQUAL(std::string("Device does not answer: No such device"),
					connection.status().error);
		device::DeviceState picture;
		CHECK(!connection.take_new_picture(picture));
	}
	{
		ConnectionFixture bench;
		Connection connection(bench.open());
		connection.reboot();
		CHECK(eventually([&] { return ended(connection); }));
		CHECK_EQUAL(std::string(""), connection.status().error);
	}
	{
		Connection connection([] -> std::unique_ptr<device::Transport> {
			throw device::Error("Device is not connected");
		});
		CHECK(eventually([&] { return ended(connection); }));
		CHECK_EQUAL(std::string("Device is not connected"),
					connection.status().error);
	}
}

// The emulated flight moves with the time of the computer.
void flight() {
	Connection connection(device::open_flight);
	connection.watch(5ms);
	device::DeviceState first, second;
	CHECK(eventually([&] { return connection.take_new_picture(first); }));
	std::this_thread::sleep_for(50ms);
	CHECK(eventually([&] { return connection.take_new_picture(second); }));
	CHECK(second.states.adsbee.gnss.longitude !=
		  first.states.adsbee.gnss.longitude);
}
}

int main() {
	return check::run({
		TEST(idle_conversation_says_hello_every_second),
		TEST(picture_holds_what_a_display_shows),
		TEST(pictures_follow_the_watch_interval),
		TEST(device_takes_the_gnss_data_it_is_handed),
		TEST(changed_watch_interval_reschedules_the_next_picture),
		TEST(unreadable_data_ends_the_conversation),
		TEST(hello_notices_a_device_that_is_gone),
		TEST(restart_ends_the_conversation),
		TEST(device_without_service_commands_gives_no_picture),
		TEST(settings),
		TEST(settings_are_written_before_a_restart_and_before_the_end),
		TEST(connection_shows_pictures_until_it_is_destroyed),
		TEST(picture_read_after_the_watch_ended_is_dropped),
		TEST(newer_settings_replace_those_that_wait),
		TEST(connection_ends_with_the_reason),
		TEST(flight),
	});
}
