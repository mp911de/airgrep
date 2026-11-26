#include "connection.hpp"

#include <atomic>
#include <utility>

namespace remote {
using namespace std::chrono_literals;

uint64_t Connection::next_id() {
	static std::atomic<uint64_t> last = 0;
	return ++last;
}

Connection::Connection(Open open)
	: thread_([this, open = std::move(open)] { run(open); }) {}

template <typename Change>
void Connection::update_and_wake(Change change) {
	{
		const std::scoped_lock lock(mutex_);
		change();
	}
	wake_.notify_all();
}

Connection::~Connection() {
	update_and_wake([&] { stopping_ = true; });
	thread_.join();
}

Connection::Status Connection::status() const {
	const std::scoped_lock lock(mutex_);
	return status_;
}

void Connection::watch(std::chrono::milliseconds interval) {
	update_and_wake([&] {
		interval_ = interval;
		if (interval == 0ms) {
			picture_.reset();
		}
	});
}

bool Connection::take_new_picture(device::DeviceState& picture,
								  PictureChange before_copy) {
	const std::scoped_lock lock(mutex_);
	if (!picture_ || taken_) {
		return false;
	}
	if (before_copy) {
		before_copy(picture, *picture_);
	}
	picture = *picture_;
	taken_ = true;
	return true;
}

bool Connection::copy_latest_picture(device::DeviceState& picture) const {
	const std::scoped_lock lock(mutex_);
	if (!picture_) {
		return false;
	}
	picture = *picture_;
	return true;
}

void Connection::reboot() {
	update_and_wake([&] { restart_ = Restart::reboot; });
}

void Connection::enter_bootloader() {
	update_and_wake([&] { restart_ = Restart::bootloader; });
}

void Connection::reset_settings() {
	update_and_wake([&] { restart_ = Restart::factory_reset; });
}

void Connection::read_settings(std::string key) {
	update_and_wake([&] {
		pending_settings_key_ = std::move(key);
		settings_read_.reset();
	});
}

std::optional<std::vector<uint8_t>> Connection::settings_read() {
	const std::scoped_lock lock(mutex_);
	return std::exchange(settings_read_, std::nullopt);
}

void Connection::write_settings(std::string key,
								std::vector<uint8_t> settings) {
	update_and_wake([&] {
		pending_settings_write_ = {std::move(key), std::move(settings)};
	});
}

void Connection::update_gnss(const adsbee::Gnss& gnss) {
	update_and_wake([&] { pending_gnss_ = gnss; });
}

void Connection::set_day(std::chrono::sys_days day) {
	const std::scoped_lock lock(mutex_);
	day_ = day;
}

std::chrono::sys_days Connection::day() const {
	const std::scoped_lock lock(mutex_);
	return day_.value_or(std::chrono::floor<std::chrono::days>(
		std::chrono::system_clock::now()));
}

void Connection::run(const Open& open) {
	std::string error;
	std::unique_ptr<device::Transport> transport;
	try {
		transport = open();
		Conversation conversation(*transport);
		converse(conversation);
	} catch (const std::exception& failure) {
		error = failure.what();
		// The protocol asks for a session reset after an error. A device that
		// is gone cannot take it and is let go.
		try {
			if (transport) {
				transport->reset_session();
			}
		} catch (const device::Error&) {
			transport.reset();
		}
	}
	const std::scoped_lock lock(mutex_);
	status_.phase = Phase::ended;
	status_.error = error;
	picture_.reset();
}

void Connection::wait(Conversation::Time until,
					  std::chrono::milliseconds interval_at_snapshot) {
	std::unique_lock lock(mutex_);
	wake_.wait_until(lock, until, [&] {
		return stopping_ || restart_ || pending_settings_key_ ||
			   pending_settings_write_ || pending_gnss_ ||
			   interval_ != interval_at_snapshot;
	});
}

void Connection::converse(Conversation& conversation) {
	for (;;) {
		Conversation::Asked asked;
		{
			const std::scoped_lock lock(mutex_);
			status_.phase = Phase::connected;
			status_.hello = conversation.hello();
			status_.supports_component_reads =
				conversation.supports_component_reads();
			asked = {.interval = interval_,
					 .restart = restart_,
					 .settings_key =
						 std::exchange(pending_settings_key_, std::nullopt),
					 .settings_write =
						 std::exchange(pending_settings_write_, std::nullopt),
					 .gnss = std::exchange(pending_gnss_, std::nullopt),
					 .stop = stopping_};
		}
		const std::chrono::milliseconds interval = asked.interval;
		const auto began = std::chrono::steady_clock::now();
		Conversation::Step step = conversation.step(began, std::move(asked));
		{
			const std::scoped_lock lock(mutex_);
			status_.hello = conversation.hello();
			if (step.settings && !pending_settings_key_) {
				settings_read_ = std::move(step.settings);
			}
			if (step.picture && interval_ > 0ms) {
				picture_ = std::move(step.picture);
				taken_ = false;
				status_.pictures++;
				status_.last_picture_read_time =
					std::chrono::duration_cast<std::chrono::microseconds>(
						std::chrono::steady_clock::now() - began);
			}
		}
		if (!step.next) {
			return;
		}
		wait(*step.next, interval);
	}
}
}
