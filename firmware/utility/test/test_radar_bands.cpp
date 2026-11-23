#include <cmath>
#include <initializer_list>
#include <numbers>
#include <vector>

#include "check.hpp"
#include "widgets/radar/radar_bands.h"

namespace {
constexpr radar_band_level_t P = RADAR_BAND_PREVENTIVE,
							 C = RADAR_BAND_CORRECTIVE, W = RADAR_BAND_WARNING,
							 OUT = RADAR_BAND_RECOVERY;
constexpr int32_t inner_radius_px = 32, outer_radius_px = 176;

radar_track_bands_t track(std::initializer_list<radar_track_band_t> bands) {
	radar_track_bands_t found;
	radar_bands_track(bands.begin(), static_cast<uint32_t>(bands.size()),
					  &found);
	return found;
}

void check(const radar_wedge_t& wedge, int start_deg, int sweep_deg,
		   radar_band_level_t level) {
	CHECK_EQUAL(start_deg, wedge.start_deg);
	CHECK_EQUAL(sweep_deg, wedge.sweep_deg);
	CHECK(wedge.level == level);
}

void check(const radar_limit_t& limit, int deg, radar_band_level_t level) {
	CHECK_EQUAL(deg, limit.deg);
	CHECK(limit.level == level);
}

void one_band() {
	CHECK_EQUAL(0u, track({}).wedge_count);
	CHECK_EQUAL(0u, track({}).limit_count);
	auto found = track({{18, 62, P}});
	CHECK_EQUAL(1u, found.wedge_count);
	check(found.wedges[0], 18, 44, P);
	CHECK_EQUAL(2u, found.limit_count);
	check(found.limits[0], 18, P);
	check(found.limits[1], 62, P);
	CHECK_EQUAL(0u, track({{90, 90, C}}).wedge_count);
	CHECK_EQUAL(0u, track({{100, 80, C}}).wedge_count);
	found = track({{17.6f, 62.4f, P}});
	CHECK_EQUAL(1u, found.wedge_count);
	check(found.wedges[0], 18, 44, P);
}

void overlapping_bands_use_more_severe_level() {
	const auto found = track({{-30, 58, P}, {-10, 36, C}});
	CHECK_EQUAL(3u, found.wedge_count);
	check(found.wedges[0], 36, 22, P);
	check(found.wedges[1], 330, 20, P);
	check(found.wedges[2], 350, 46, C);
	CHECK_EQUAL(4u, found.limit_count);
	check(found.limits[0], 36, C);
	check(found.limits[1], 58, P);
	check(found.limits[2], 330, P);
	check(found.limits[3], 350, C);
	const auto turned = track({{-10, 36, C}, {-30, 58, P}});
	CHECK_EQUAL(found.wedge_count, turned.wedge_count);
	CHECK_EQUAL(found.limit_count, turned.limit_count);
	for (size_t i = 0; i < found.wedge_count && i < turned.wedge_count; ++i) {
		const auto w = found.wedges[i];
		check(turned.wedges[i], w.start_deg, w.sweep_deg, w.level);
	}
	for (size_t i = 0; i < found.limit_count && i < turned.limit_count; ++i) {
		const auto limit = found.limits[i];
		check(turned.limits[i], limit.deg, limit.level);
	}
}

void three_levels() {
	const auto found = track({{276, 310, C}, {-32, 96, C}, {-14, 70, W}});
	CHECK_EQUAL(4u, found.wedge_count);
	check(found.wedges[0], 70, 26, C);
	check(found.wedges[1], 276, 34, C);
	check(found.wedges[2], 328, 18, C);
	check(found.wedges[3], 346, 84, W);
	CHECK_EQUAL(6u, found.limit_count);
	check(found.limits[0], 70, W);
	check(found.limits[5], 346, W);
}

// The engine cuts a Band at north: both parts are one wedge, turned by the
// own track or not.
void across_north() {
	auto found = track({{0, 40, C}, {300, 360, C}});
	CHECK_EQUAL(1u, found.wedge_count);
	check(found.wedges[0], 300, 100, C);
	CHECK_EQUAL(2u, found.limit_count);
	found = track({{-90, -50, C}, {210, 270, C}});
	CHECK_EQUAL(1u, found.wedge_count);
	check(found.wedges[0], 210, 100, C);
}

void all_around() {
	auto found = track({{0, 360, W}});
	CHECK_EQUAL(1u, found.wedge_count);
	check(found.wedges[0], 0, 360, W);
	CHECK_EQUAL(0u, found.limit_count);
	found = track({{-100, 80, W}, {80, 260, W}});
	CHECK_EQUAL(1u, found.wedge_count);
	check(found.wedges[0], 0, 360, W);
	CHECK_EQUAL(0u, found.limit_count);
}

void way_out() {
	auto found = track({{0, 360, W}, {148, 186, OUT}});
	CHECK_EQUAL(2u, found.wedge_count);
	check(found.wedges[0], 148, 38, OUT);
	check(found.wedges[1], 186, 322, W);
	CHECK_EQUAL(0u, found.limit_count);
	found = track({{0, 148, W}, {148, 186, OUT}, {186, 360, W}});
	CHECK_EQUAL(2u, found.wedge_count);
	check(found.wedges[1], 186, 322, W);
	CHECK_EQUAL(0u, found.limit_count);
}

void as_many_as_fit() {
	radar_track_band_t bands[RADAR_MAX_BANDS];
	for (int i = 0; i < RADAR_MAX_BANDS; i++) {
		const float from = static_cast<float>(i) * 11.0f;
		bands[i] = {from, from + 5, i % 2 ? P : C};
	}
	radar_track_bands_t found;
	radar_bands_track(bands, RADAR_MAX_BANDS, &found);
	CHECK_EQUAL(static_cast<uint32_t>(RADAR_MAX_BANDS), found.wedge_count);
	CHECK_EQUAL(static_cast<uint32_t>(RADAR_MAX_WEDGES), found.limit_count);
}

std::vector<radar_stretch_t> tape(
	std::initializer_list<radar_tape_band_t> bands) {
	std::vector<radar_stretch_t> found(bands.size());
	found.resize(radar_bands_tape(
		bands.begin(), static_cast<uint32_t>(bands.size()), 352, found.data()));
	return found;
}

void check(const radar_stretch_t& stretch, int y, int height,
		   radar_band_level_t level) {
	CHECK_EQUAL(y, stretch.y);
	CHECK_EQUAL(height, stretch.height);
	CHECK(stretch.level == level);
}

void stretches() {
	constexpr float half_span_ft = 2000;
	CHECK_EQUAL(0u, tape({}).size());
	auto found = tape({{-100 / half_span_ft, 800 / half_span_ft, P}});
	CHECK_EQUAL(1u, found.size());
	check(found[0], 106, 79, P);
	found = tape({{-200 / half_span_ft, 600 / half_span_ft, C},
				  {-450 / half_span_ft, 850 / half_span_ft, P}});
	CHECK_EQUAL(2u, found.size());
	check(found[0], 101, 115, P);
	check(found[1], 123, 71, C);
}

void beyond_the_tape() {
	auto found = tape({{-1, 1, W}});
	check(found[0], 0, 352, W);
	found = tape({{0.5f, 3, C}, {-4, -0.5f, P}});
	CHECK_EQUAL(2u, found.size());
	check(found[0], 264, 88, P);
	check(found[1], 0, 88, C);
	CHECK_EQUAL(0u, tape({{1.5f, 2, C}}).size());
	CHECK_EQUAL(0u, tape({{-0.5f, 0.5f, OUT}}).size());
}

void check_point(radar_point_t point, float x, float y) {
	CHECK(std::abs(point.x - x) < 0.01f);
	CHECK(std::abs(point.y - y) < 0.01f);
}

void quarter_turn_wedge_perimeter() {
	const radar_wedge_t wedge = {0, 90, OUT};
	const float quarter = std::numbers::pi_v<float> / 2;
	const float length =
		radar_bands_edge_length(&wedge, inner_radius_px, outer_radius_px);
	const float radial_px = outer_radius_px - inner_radius_px;
	const float inner_arc_px = quarter * inner_radius_px,
				outer_arc_px = quarter * outer_radius_px;
	CHECK(std::abs(length - (2 * radial_px + inner_arc_px + outer_arc_px)) <
		  0.01f);
	check_point(radar_bands_edge(&wedge, inner_radius_px, outer_radius_px, 0),
				0, -32);
	check_point(radar_bands_edge(&wedge, inner_radius_px, outer_radius_px, 72),
				0, -104);
	check_point(radar_bands_edge(&wedge, inner_radius_px, outer_radius_px, 144),
				0, -176);
	check_point(radar_bands_edge(&wedge, inner_radius_px, outer_radius_px,
								 144 + quarter * 88),
				124.45f, -124.45f);
	check_point(radar_bands_edge(&wedge, inner_radius_px, outer_radius_px,
								 144 + quarter * 176),
				176, 0);
	check_point(radar_bands_edge(&wedge, inner_radius_px, outer_radius_px,
								 288 + quarter * 176),
				32, 0);
	check_point(
		radar_bands_edge(&wedge, inner_radius_px, outer_radius_px, length), 0,
		-32);
}
}

int main() {
	return check::run({
		TEST(one_band),
		TEST(overlapping_bands_use_more_severe_level),
		TEST(three_levels),
		TEST(across_north),
		TEST(all_around),
		TEST(way_out),
		TEST(as_many_as_fit),
		TEST(stretches),
		TEST(beyond_the_tape),
		TEST(quarter_turn_wedge_perimeter),
	});
}
