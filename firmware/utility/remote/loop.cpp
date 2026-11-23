#include "loop.hpp"

#include <utility>

namespace remote {
Loop::Loop() : thread_([this] { run(); }) {}

Loop::~Loop() {
	request_stop();
	thread_.join();
}

void Loop::request_stop() {
	{
		const std::scoped_lock lock(mutex_);
		stopping_ = true;
	}
	wake_.notify_all();
}

void Loop::submit(Action action) {
	{
		const std::scoped_lock lock(mutex_);
		if (stopping_) {
			return;
		}
		submitted_.push_back(std::move(action));
	}
	wake_.notify_all();
}

bool Loop::wait_idle(std::chrono::milliseconds timeout) {
	std::unique_lock lock(mutex_);
	return wake_.wait_for(lock, timeout, [&] { return submitted_.empty(); });
}

void Loop::run() {
	for (;;) {
		Action action;
		{
			std::unique_lock lock(mutex_);
			wake_.wait(lock, [&] { return stopping_ || !submitted_.empty(); });
			if (stopping_) {
				std::deque<Action> discarded;
				discarded.swap(submitted_);
				lock.unlock();
				wake_.notify_all();
				return;
			}
			action = std::move(submitted_.front());
		}
		action();
		{
			const std::scoped_lock lock(mutex_);
			submitted_.pop_front();
			if (submitted_.empty()) {
				wake_.notify_all();
			}
		}
	}
}
}
