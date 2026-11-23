#include <initializer_list>
#include <vector>

#include "check.hpp"
#include "widgets/radar/radar_sections.h"

namespace {
std::vector<radar_section_t> sections(std::initializer_list<int16_t> bearings) {
	std::vector<int16_t> sorted(bearings);
	radar_section_t found[RADAR_MAX_SECTIONS];
	const uint32_t count = radar_sections(
		sorted.data(), static_cast<uint32_t>(sorted.size()), found);
	return {found, found + count};
}

void check(const radar_section_t& section, int start_deg, int sweep_deg) {
	CHECK_EQUAL(start_deg, section.start_deg);
	CHECK_EQUAL(sweep_deg, section.sweep_deg);
}

void one_target() {
	CHECK_EQUAL(0u, sections({}).size());
	auto found = sections({300});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 290, 20);
	found = sections({300, 300});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 290, 20);
}

void gap() {
	constexpr int reference = 90, width = 20;
	auto found = sections({reference + width + 14, reference});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 80, 54);
	found = sections({reference + width + 15, reference});
	CHECK_EQUAL(2u, found.size());
	check(found[0], 80, 20);
	check(found[1], 115, 20);
	found = sections({290, 304});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 280, 34);
}

void across_north() {
	auto found = sections({350, 10});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 340, 40);
	found = sections({5});
	check(found[0], 355, 20);
	constexpr int16_t isolated_south = 180;
	found = sections({330, 0, 30, isolated_south});
	CHECK_EQUAL(2u, found.size());
	check(found[0], 170, 20);
	check(found[1], 320, 80);
}

void whole_ring() {
	auto found =
		sections({0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 0, 360);
	found = sections({0, 35, 70, 105, 140, 175, 210, 245, 280, 315});
	CHECK_EQUAL(static_cast<size_t>(RADAR_MAX_SECTIONS), found.size());
	for (const radar_section_t& section : found) {
		CHECK_EQUAL(20, section.sweep_deg);
	}
}
}

int main() {
	return check::run({
		TEST(one_target),
		TEST(gap),
		TEST(across_north),
		TEST(whole_ring),
	});
}
