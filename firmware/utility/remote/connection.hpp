#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "conversation.hpp"

namespace remote {
// The one Conversation with a device, on a thread of its own and at the time
// of the computer. It ends with the first failure, a new Connection starts
// over.
class Connection {
   public:
	enum class Phase { connecting, connected, ended };

	struct Status {
		Phase phase = Phase::connecting;
		// Why it ended. Empty after a reboot that was asked for.
		std::string error;
		device::Hello hello{};
		bool supports_component_reads = false;
		std::chrono::microseconds last_picture_read_time{0};
		uint64_t pictures = 0;
	};

	using Open = std::function<std::unique_ptr<device::Transport>()>;
	// open runs on the connection's thread.
	explicit Connection(Open open);
	~Connection();
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) = delete;
	Connection& operator=(Connection&&) = delete;

	Status status() const;
	// Unlike its address, never that of another Connection.
	uint64_t id() const { return id_; }
	// A picture every interval, or none for a zero interval. A read that takes
	// longer than the interval is followed by the next one at once.
	void watch(std::chrono::milliseconds interval);
	bool take_new_picture(device::DeviceState& picture);
	bool copy_latest_picture(device::DeviceState& picture) const;
	// Asks the device to restart, or to restart into its bootloader. The
	// Connection ends with it.
	void reboot();
	void enter_bootloader();
	// Removes every setting the device stores, then restarts it: a factory
	// reset. Without the removal there is no restart, the Connection ends
	// with the reason.
	void reset_settings();

	// The settings of one display, as the bytes of the struct the device
	// stores under the key. read_settings asks for them, settings_read hands
	// them out once: empty when the device stores none under the key, nothing
	// while there is no answer.
	void read_settings(std::string key);
	std::optional<std::vector<uint8_t>> settings_read();
	// Writes them to the device. Of two that wait the newer is written. A
	// device that refuses them ends the Connection.
	void write_settings(std::string key, std::vector<uint8_t> settings);
	// Hands the device GNSS data in place of what its receiver says. Of two
	// that wait the newer is sent.
	void update_gnss(const adsbee::Gnss& gnss);
	// The device says the time of day and no date. The day its time belongs
	// to: today, unless one is set.
	void set_day(std::chrono::sys_days day);
	std::chrono::sys_days day() const;

   private:
	template <typename Change>
	void update_and_wake(Change change);
	void run(const Open& open);
	void converse(Conversation& conversation);
	void wait(Conversation::Time until,
			  std::chrono::milliseconds interval_at_snapshot);

	mutable std::mutex mutex_;
	std::condition_variable wake_;
	Status status_;
	std::chrono::milliseconds interval_{0};
	using Restart = Conversation::Restart;
	std::optional<Restart> restart_;
	std::optional<std::string> pending_settings_key_;
	std::optional<std::vector<uint8_t>> settings_read_;
	std::optional<Conversation::SettingsWrite> pending_settings_write_;
	std::optional<adsbee::Gnss> pending_gnss_;
	std::optional<std::chrono::sys_days> day_;
	std::optional<device::DeviceState> picture_;
	bool taken_ = false;
	bool stopping_ = false;
	const uint64_t id_ = next_id();
	static uint64_t next_id();
	std::thread thread_;
};
}
