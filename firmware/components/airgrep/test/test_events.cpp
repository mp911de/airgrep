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
using events::of;
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
}  // namespace

CASE(order, "Subscribers get an event in subscription order") {
	Bus bus;
	Log log;
	Tagged a{&log, 'a'}, b{&log, 'b'}, c{&log, 'c'};
	TEST_ASSERT_TRUE(bus.subscribe(of(Type::clock_set), record, &a));
	TEST_ASSERT_TRUE(bus.subscribe(of(Type::clock_set), record, &b));
	TEST_ASSERT_TRUE(bus.subscribe(of(Type::clock_set), record, &c));
	bus.publish(Type::clock_set);
	TEST_ASSERT_EQUAL_STRING("abc", log.text);
}

CASE(filter, "A subscriber gets only the types it asked for") {
	Bus bus;
	int fix = 0, usb = 0;
	bus.subscribe(of(Type::fix_acquired, Type::fix_lost), count, &fix);
	bus.subscribe(of(Type::usb_plugged), count, &usb);
	bus.publish({.type = Type::fix_acquired, .fix = {2}});
	bus.publish({.type = Type::fix_lost, .fix = {4}});
	bus.publish({.type = Type::usb_plugged, .usb = {1500}});
	bus.publish(Type::clock_set);
	TEST_ASSERT_EQUAL(2, fix);
	TEST_ASSERT_EQUAL(1, usb);
}

CASE(payload, "The handler reads the payload that was published") {
	Bus bus;
	static events::Health seen;
	bus.subscribe(of(Type::health_changed),
				  [](const Event& event, void*) { seen = event.health; });
	bus.publish({.type = Type::health_changed, .health = {0x02, 0x62}});
	TEST_ASSERT_EQUAL_HEX16(0x02, seen.before);
	TEST_ASSERT_EQUAL_HEX16(0x62, seen.now);
}

CASE(no_replay, "A late subscriber gets nothing published before") {
	Bus bus;
	int seen = 0;
	bus.publish(Type::clock_set);
	bus.subscribe(of(Type::clock_set), count, &seen);
	TEST_ASSERT_EQUAL(0, seen);
}

CASE(full, "A full bus refuses a subscriber and keeps delivering") {
	Bus bus;
	int seen = 0;
	for (size_t i = 0; i < events::kMaxSubscribers; i++) {
		TEST_ASSERT_TRUE(bus.subscribe(of(Type::clock_set), count, &seen));
	}
	TEST_ASSERT_FALSE(bus.subscribe(of(Type::clock_set), count, &seen));
	TEST_ASSERT_FALSE(bus.subscribe(of(Type::clock_set), nullptr));
	bus.publish(Type::clock_set);
	TEST_ASSERT_EQUAL(events::kMaxSubscribers, seen);
}

CASE(nested, "A handler may publish and subscribe") {
	static Bus bus;
	static int inner;
	inner = 0;
	bus.subscribe(of(Type::usb_plugged), [](const Event&, void*) {
		bus.subscribe(of(Type::clock_set), count, &inner);
		bus.publish(Type::clock_set);
	});
	bus.publish({.type = Type::usb_plugged, .usb = {0}});
	TEST_ASSERT_EQUAL(1, inner);
}

CASE(change, "Only a change is reported") {
	events::Supply last{};
	const events::Supply usb_c{charger::Supply::usb_c, 3000};
	TEST_ASSERT_TRUE(events::changed(last, usb_c));
	TEST_ASSERT_FALSE(events::changed(last, usb_c));
	TEST_ASSERT_TRUE(events::changed(last, {charger::Supply::usb_c, 1500}));
}

CASE(fix_rule, "A fix is lost only after it was held") {
	bool held = false;
	TEST_ASSERT_FALSE(events::changed(held, false));
	TEST_ASSERT_TRUE(events::changed(held, true));
	TEST_ASSERT_FALSE(events::changed(held, true));
	TEST_ASSERT_TRUE(events::changed(held, false));
	TEST_ASSERT_FALSE(events::changed(held, false));
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
