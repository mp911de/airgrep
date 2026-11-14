#include "events.hpp"

namespace events {
bool Bus::subscribe(uint32_t types, Handler handler, void* context) {
	if (!handler) {
		return false;
	}
	size_t index = claimed_.load(std::memory_order_relaxed);
	do {
		if (index == kMaxSubscribers) {
			return false;
		}
	} while (!claimed_.compare_exchange_weak(index, index + 1,
											 std::memory_order_acq_rel));
	Entry& entry = entries_[index];
	entry.context = context;
	entry.types = types;
	entry.handler.store(handler, std::memory_order_release);
	return true;
}

void Bus::publish(const Event& event) {
	const size_t claimed = claimed_.load(std::memory_order_acquire);
	for (size_t i = 0; i < claimed; i++) {
		const Entry& entry = entries_[i];
		const Handler handler = entry.handler.load(std::memory_order_acquire);
		if (handler && entry.types & of(event.type)) {
			handler(event, entry.context);
		}
	}
}

void Bus::publish(Type type) {
	Event event{};
	event.type = type;
	publish(event);
}

namespace {
Bus bus;
}

bool subscribe(uint32_t types, Handler handler, void* context) {
	return bus.subscribe(types, handler, context);
}

void publish(const Event& event) { bus.publish(event); }

void publish(Type type) { bus.publish(type); }
}  // namespace events
