#include <cstdint>
#include <cstring>

#include "display/settings_store.hpp"
#include "display_settings.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[settings]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

namespace {
using display::Settings;

uint8_t kept[sizeof(Settings)];
size_t kept_size;
bool flash_works;
size_t reads;

bool read_flash(const char* key, void* out, size_t size) {
	reads++;
	if (std::strcmp(key, display::kSettingsKey) || size != kept_size) {
		return false;
	}
	std::memcpy(out, kept, size);
	return true;
}

bool write_flash(const char* key, const void* blob, size_t size) {
	if (!flash_works || std::strcmp(key, display::kSettingsKey) ||
		size > sizeof kept) {
		return false;
	}
	std::memcpy(kept, blob, size);
	kept_size = size;
	return true;
}

Settings in_flash() {
	TEST_ASSERT_EQUAL_UINT(sizeof(Settings), kept_size);
	Settings settings;
	std::memcpy(&settings, kept, sizeof settings);
	return settings;
}

display::SettingsStore<Settings> fresh() {
	kept_size = 0;
	flash_works = true;
	reads = 0;
	return {display::kSettingsKey, {read_flash, write_flash}};
}

}

CASE(loaded, "Display settings load what is stored, or the defaults") {
	display::SettingsStore<Settings> store = fresh();
	store.load();
	TEST_ASSERT_EQUAL_UINT8(1, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(0, store.settings().conflict_indicator);
	TEST_ASSERT_EQUAL_UINT(0, kept_size);

	Settings before;
	before.radar_range_index = 3;
	before.conflict_indicator = 1;
	std::memcpy(kept, &before, sizeof before);
	kept_size = sizeof before;
	store.load();
	TEST_ASSERT_EQUAL_UINT8(3, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(1, store.settings().conflict_indicator);

	before.version++;
	std::memcpy(kept, &before, sizeof before);
	store.load();
	TEST_ASSERT_EQUAL_UINT8(1, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(0, store.settings().conflict_indicator);

	before.version = display::kSettingsVersion;
	before.conflict_indicator = 2;
	std::memcpy(kept, &before, sizeof before);
	store.load();
	TEST_ASSERT_EQUAL_UINT8(1, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(0, store.settings().conflict_indicator);
}

CASE(set, "A change is in use and stored, and read from RAM from then on") {
	display::SettingsStore<Settings> store = fresh();
	store.load();
	const size_t loaded = reads;
	Settings next;
	next.conflict_indicator = 1;
	next.radar_range_index = 4;
	TEST_ASSERT_TRUE(store.save(next));
	TEST_ASSERT_EQUAL_UINT8(1, store.settings().conflict_indicator);
	TEST_ASSERT_EQUAL_UINT8(4, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(1, in_flash().conflict_indicator);
	TEST_ASSERT_EQUAL_UINT8(4, in_flash().radar_range_index);
	TEST_ASSERT_EQUAL_UINT(loaded, reads);

	kept_size = 0;
	TEST_ASSERT_EQUAL_UINT8(4, store.settings().radar_range_index);
	store.load();
	TEST_ASSERT_EQUAL_UINT8(1, store.settings().radar_range_index);
}

CASE(refused, "A change that is not valid or not stored changes nothing") {
	display::SettingsStore<Settings> store = fresh();
	store.load();
	Settings next;
	next.radar_range_index = 2;
	TEST_ASSERT_TRUE(store.save(next));

	Settings odd = next;
	odd.radar_range_index = 5;
	TEST_ASSERT_FALSE(store.save(odd));
	Settings newer = next;
	newer.version++;
	TEST_ASSERT_FALSE(store.save(newer));
	TEST_ASSERT_EQUAL_UINT8(2, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(2, in_flash().radar_range_index);

	flash_works = false;
	next.radar_range_index = 3;
	TEST_ASSERT_FALSE(store.save(next));
	TEST_ASSERT_EQUAL_UINT8(2, store.settings().radar_range_index);
	TEST_ASSERT_EQUAL_UINT8(2, in_flash().radar_range_index);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(loaded);
	RUN_TEST(set);
	RUN_TEST(refused);
	return UNITY_END();
}
#endif
