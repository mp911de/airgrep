#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace remote {
// The event loop of a host: one thread that runs Tasks in submission order,
// as the loop of the firmware does. A Task here is its action alone. The loop
// of the firmware is made of FreeRTOS and does not build on a host.
class Loop {
   public:
	using Action = std::function<void()>;

	Loop();
	// Waits for the Task that runs. Those that have yet to run never do.
	~Loop();
	Loop(const Loop&) = delete;
	Loop& operator=(const Loop&) = delete;
	Loop(Loop&&) = delete;
	Loop& operator=(Loop&&) = delete;

	void submit(Action action);
	bool wait_idle(std::chrono::milliseconds timeout);
	void request_stop();

   private:
	void run();

	std::mutex mutex_;
	std::condition_variable wake_;
	std::deque<Action> submitted_;
	bool stopping_ = false;
	std::thread thread_;
};
}
