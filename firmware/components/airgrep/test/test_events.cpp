#include "events/events.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[events]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

using events::Bus;
using events::Event;
using events::mask_for;
using events::Type;

namespace {
struct Log {
	char text[16]{};
	size_t length = 0;
};

struct Tagged {
	Log* log;
	char tag;
};

void record(const Event&, void* context) {
	auto* tagged = static_cast<Tagged*>(context);
	tagged->log->text[tagged->log->length++] = tagged->tag;
}

void count(const Event&, void* context) { ++*static_cast<int*>(context); }
}

CASE(order, "Subscribers get an event in subscription order") {
	Bus bus;
	Log log;
	Tagged a{&log, 'a'}, b{&log, 'b'}, c{&log, 'c'};
	TEST_ASSERT_TRUE(bus.subscribe(mask_for(Type::clock_set), record, &a));
	TEST_ASSERT_TRUE(bus.subscribe(mask_for(Type::clock_set), record, &b));
	TEST_ASSERT_TRUE(bus.subscribe(mask_for(Type::clock_set), record, &c));
	bus.publish(Event::clock_set());
	TEST_ASSERT_EQUAL_STRING("abc", log.text);
}

CASE(filter, "A subscriber gets only the types it asked for") {
	Bus bus;
	int fix = 0, usb = 0;
	bus.subscribe(mask_for(Type::fix_acquired, Type::fix_lost), count, &fix);
	bus.subscribe(mask_for(Type::usb_plugged), count, &usb);
	bus.publish(Event::fix_changed(true, {adsbee::AntennaStatus::ok}));
	bus.publish(Event::fix_changed(false, {adsbee::AntennaStatus::open}));
	bus.publish(Event::usb_attachment_changed(true, {1500}));
	bus.publish(Event::clock_set());
	TEST_ASSERT_EQUAL(2, fix);
	TEST_ASSERT_EQUAL(1, usb);
}

CASE(payload, "The handler reads the payload that was published") {
	Bus bus;
	static events::Health seen;
	bus.subscribe(mask_for(Type::health_changed),
				  [](const Event& event, void*) { seen = event.data.health; });
	bus.publish(Event::health_changed({0x02, 0x62}));
	TEST_ASSERT_EQUAL_HEX16(0x02, seen.previous_mask);
	TEST_ASSERT_EQUAL_HEX16(0x62, seen.current_mask);
}

CASE(no_replay, "A late subscriber gets nothing published before") {
	Bus bus;
	int seen = 0;
	bus.publish(Event::clock_set());
	bus.subscribe(mask_for(Type::clock_set), count, &seen);
	TEST_ASSERT_EQUAL(0, seen);
}

CASE(full, "A full bus refuses a subscriber and keeps delivering") {
	Bus bus;
	int seen = 0;
	for (size_t i = 0; i < events::kMaxSubscribers; i++) {
		TEST_ASSERT_TRUE(
			bus.subscribe(mask_for(Type::clock_set), count, &seen));
	}
	TEST_ASSERT_FALSE(bus.subscribe(mask_for(Type::clock_set), count, &seen));
	TEST_ASSERT_FALSE(bus.subscribe(mask_for(Type::clock_set), nullptr));
	bus.publish(Event::clock_set());
	TEST_ASSERT_EQUAL(events::kMaxSubscribers, seen);
}

CASE(nested, "A handler may publish and subscribe") {
	struct Nested {
		Bus bus;
		int inner = 0;
	} nested;
	nested.bus.subscribe(
		mask_for(Type::usb_plugged),
		[](const Event&, void* context) {
			auto* outer = static_cast<Nested*>(context);
			outer->bus.subscribe(mask_for(Type::clock_set), count,
								 &outer->inner);
			outer->bus.publish(Event::clock_set());
		},
		&nested);
	nested.bus.publish(Event::usb_attachment_changed(true, {0}));
	TEST_ASSERT_EQUAL(1, nested.inner);
}

CASE(change, "Only a change is reported") {
	events::Supply last{};
	const events::Supply usb_c{charger::Supply::usb_c, 3000};
	TEST_ASSERT_TRUE(events::update_if_changed(last, usb_c));
	TEST_ASSERT_FALSE(events::update_if_changed(last, usb_c));
	TEST_ASSERT_TRUE(
		events::update_if_changed(last, {charger::Supply::usb_c, 1500}));
}

CASE(fix_rule, "A fix is lost only after it was held") {
	bool held = false;
	TEST_ASSERT_FALSE(events::update_if_changed(held, false));
	TEST_ASSERT_TRUE(events::update_if_changed(held, true));
	TEST_ASSERT_FALSE(events::update_if_changed(held, true));
	TEST_ASSERT_TRUE(events::update_if_changed(held, false));
	TEST_ASSERT_FALSE(events::update_if_changed(held, false));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(order);
	RUN_TEST(filter);
	RUN_TEST(payload);
	RUN_TEST(no_replay);
	RUN_TEST(full);
	RUN_TEST(nested);
	RUN_TEST(change);
	RUN_TEST(fix_rule);
	return UNITY_END();
}
#endif
