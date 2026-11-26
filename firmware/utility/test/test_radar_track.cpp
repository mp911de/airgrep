#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <tuple>
#include <vector>

#include "check.hpp"
#include "navigation/units.h"
#include "widgets/radar/radar_track.h"

namespace {
constexpr float outer_ring_radius_px = 176;

float distance(radar_point_t from, radar_point_t to) {
	return std::hypot(to.x - from.x, to.y - from.y);
}

bool near(float expected, float actual) {
	return std::fabs(expected - actual) < 0.05f;
}

std::vector<radar_point_t> eastbound_dots(float speed_kt, int range_nm,
										  float turn_dps = 0) {
	radar_point_t drawn[RADAR_DOT_COUNT];
	const uint32_t count = radar_track_dots(
		speed_kt / 3600 * outer_ring_radius_px / static_cast<float>(range_nm),
		90, turn_dps, drawn);
	return {drawn, drawn + count};
}

void dot_counts() {
	CHECK_EQUAL(5u, eastbound_dots(250, 2).size());
	CHECK_EQUAL(5u, eastbound_dots(100, 2).size());
	CHECK_EQUAL(5u, eastbound_dots(250, 4).size());
	CHECK_EQUAL(3u, eastbound_dots(60, 2).size());
	CHECK_EQUAL(3u, eastbound_dots(100, 4).size());
	CHECK_EQUAL(3u, eastbound_dots(250, 8).size());
	CHECK_EQUAL(2u, eastbound_dots(60, 4).size());
	CHECK_EQUAL(2u, eastbound_dots(250, 16).size());
	CHECK_EQUAL(1u, eastbound_dots(60, 8).size());
	CHECK_EQUAL(1u, eastbound_dots(15, 2).size());
}

void dot_places() {
	for (auto [speed, range, every] :
		 {std::tuple{250.0f, 2, 1}, {100.0f, 4, 2}, {250.0f, 16, 4}}) {
		std::vector<radar_point_t> drawn = eastbound_dots(speed, range);
		constexpr float dot_interval_s = 6;
		const float step = speed / 3600 * dot_interval_s *
						   outer_ring_radius_px / static_cast<float>(range);
		for (size_t i = 0; i < drawn.size(); i++) {
			CHECK(near(RADAR_TRACK_NOSE + step * (1 + i * every), drawn[i].x));
			CHECK(near(0, drawn[i].y));
		}
	}
	for (const int range : {2, 4, 8, 16, 32}) {
		for (const float speed : {5.0f, 15.0f, 60.0f, 100.0f, 250.0f, 450.0f}) {
			for (const float turn : {0.0f, 6.0f, -3.0f}) {
				std::vector<radar_point_t> drawn =
					eastbound_dots(speed, range, turn);
				CHECK(!drawn.empty());
				for (size_t i = 0; i < drawn.size(); i++) {
					CHECK(distance({0, 0}, drawn[i]) >=
						  RADAR_TRACK_CLEAR - 0.05f);
					CHECK(i == 0 || distance(drawn[i - 1], drawn[i]) >=
										RADAR_DOT_STEP_SOME);
				}
			}
		}
	}
}

void fixed_dot() {
	for (const int range : {4, 16, 32}) {
		std::vector<radar_point_t> slow_straight = eastbound_dots(5, range);
		CHECK_EQUAL(1u, slow_straight.size());
		CHECK(near(RADAR_TRACK_CLEAR, slow_straight[0].x));
		CHECK(near(0, slow_straight[0].y));
	}
	std::vector<radar_point_t> tight_turn = eastbound_dots(100, 8, 6);
	CHECK_EQUAL(1u, tight_turn.size());
	CHECK(near(RADAR_TRACK_CLEAR, tight_turn[0].x));
	CHECK(near(0, tight_turn[0].y));
	// In a wide turn the dots lie on the arc, to the side the target turns
	// to. East and a turn to the right lead south, down.
	radar_point_t wide[RADAR_DOT_COUNT];
	CHECK_EQUAL(2u, radar_track_dots(0.5f, 90, 1, wide));
	CHECK(near(RADAR_TRACK_CLEAR, distance({0, 0}, wide[0])));
	CHECK(wide[0].x > 0 && wide[0].y > 1 && wide[1].y > wide[0].y);
	CHECK_EQUAL(2u, radar_track_dots(0.5f, 90, -1, wide));
	CHECK(near(RADAR_TRACK_CLEAR, distance({0, 0}, wide[0])));
	CHECK(wide[0].x > 0 && wide[0].y < -1 && wide[1].y < wide[0].y);
}

std::vector<radar_point_t> westward_history(int places, float spacing_px) {
	std::vector<radar_point_t> path;
	path.reserve(static_cast<size_t>(places));
	for (int i = 0; i < places; i++) {
		path.push_back({-spacing_px * static_cast<float>(i), 0});
	}
	return path;
}

std::vector<radar_tail_part_t> tail(std::vector<radar_point_t> path) {
	std::vector<radar_tail_part_t> parts(path.size() + RADAR_TAIL_STEPS);
	parts.resize(radar_track_tail(
		path.data(), static_cast<uint32_t>(path.size()), parts.data()));
	return parts;
}

void tail_length() {
	const auto clipped = tail(westward_history(61, 2));
	const float clipped_step_px = RADAR_MAX_TAIL_LENGTH_PX / 5.f;
	CHECK_EQUAL(size_t{RADAR_TAIL_STEPS}, clipped.size());
	for (size_t i = 0; i < clipped.size(); i++) {
		CHECK_EQUAL(i, clipped[i].step);
		CHECK(near(-clipped_step_px * i, clipped[i].from.x));
		CHECK(near(-clipped_step_px * (i + 1), clipped[i].to.x));
	}
	const auto unclipped = tail(westward_history(61, 0.5f));
	CHECK_EQUAL(size_t{RADAR_TAIL_STEPS}, unclipped.size());
	CHECK(near(-30, unclipped.back().to.x));
	constexpr float unclipped_step_px = 30.f / 5;
	CHECK(near(-unclipped_step_px, unclipped[0].to.x));
	CHECK(tail(westward_history(1, 2)).empty());
	CHECK(tail({}).empty());
	CHECK(tail(westward_history(10, 0)).empty());
}

void tail_shape() {
	std::vector<radar_point_t> path = westward_history(11, 2);
	for (int i = 1; i <= 10; i++) {
		path.push_back({-20, 2.0f * static_cast<float>(i)});
	}
	std::vector<radar_tail_part_t> parts = tail(path);
	float length = 0;
	for (size_t i = 0; i < parts.size(); i++) {
		length += distance(parts[i].from, parts[i].to);
		CHECK(i == 0 || parts[i].step >= parts[i - 1].step);
		CHECK(i == 0 || near(0, distance(parts[i - 1].to, parts[i].from)));
	}
	CHECK(near(40, length));
	CHECK(near(-20, parts.back().to.x));
	CHECK(near(20, parts.back().to.y));
	path = westward_history(5, 4);
	const auto held_position = path[2];
	path.insert(path.begin() + 2, held_position);
	CHECK(near(-16, tail(path).back().to.x));
}

struct Pixels {
	static constexpr int kReach = 100;
	uint8_t opacity[2 * kReach][2 * kReach] = {};
	int runs = 0;
	float sum = 0;
	bool twice = false;
	static void run(void* user, int32_t x, int32_t y, int32_t length,
					uint8_t opa) {
		Pixels& pixels = *static_cast<Pixels*>(user);
		pixels.runs++;
		for (int32_t i = x; i < x + length; i++) {
			pixels.twice |= pixels.opacity[y + kReach][i + kReach] != 0;
			pixels.opacity[y + kReach][i + kReach] = opa;
			pixels.sum += static_cast<float>(opa) / 255.0f;
		}
	}
};

Pixels pixels(const std::vector<radar_point_t>& path) {
	std::vector<radar_tail_part_t> parts = tail(path);
	Pixels drawn;
	radar_track_tail_runs(parts.data(), static_cast<uint32_t>(parts.size()), 3,
						  Pixels::run, &drawn);
	return drawn;
}

void tail_pixels() {
	// A straight tail of 30 px, 3 px wide, to the west on the line between
	// two rows: both rows in full, half of the row above and of the one below.
	// It fades in five steps of 6 px.
	const Pixels straight = pixels(westward_history(16, 2));
	for (int step = 0; step < RADAR_TAIL_STEPS; step++) {
		const int full = 255 * (RADAR_TAIL_STEPS - step) / RADAR_TAIL_STEPS;
		const int x = Pixels::kReach - 3 - 6 * step;
		CHECK_EQUAL(full, straight.opacity[Pixels::kReach - 1][x]);
		CHECK_EQUAL(full, straight.opacity[Pixels::kReach][x]);
		CHECK(std::abs(full / 2 - straight.opacity[Pixels::kReach - 2][x]) <=
			  1);
		CHECK(std::abs(full / 2 - straight.opacity[Pixels::kReach + 1][x]) <=
			  1);
		CHECK_EQUAL(0, straight.opacity[Pixels::kReach - 3][x]);
	}
	// A stretch of a row at one opacity is one run: four rows, each with its
	// five steps and the pixels where one step ends in the next.
	CHECK(straight.runs <= 4 * (RADAR_TAIL_STEPS + 4));
	// A tail that turns is as heavy as a straight one of its length, within
	// its round ends: no pixel is given twice and none is darker where two
	// parts meet.
	std::vector<radar_point_t> turning;
	for (int i = 0; i <= 30; i++) {
		const float angle =
			static_cast<float>(i * 6) * units::radians_per_degree<float>;
		turning.push_back({12 * (std::cos(angle) - 1), 12 * std::sin(angle)});
	}
	const Pixels turned = pixels(turning);
	CHECK(!turned.twice);
	const float length = 12 * std::numbers::pi_v<float>;
	const float weight = 3 * length * (1 + 2 + 3 + 4 + 5) / 5 / 5;
	CHECK(std::fabs(turned.sum - weight) < 0.1f * weight);
}

void tail_reversal_pixels() {
	const Pixels drawn = pixels({{0, 0}, {10, 0}, {5, 0}});
	CHECK(drawn.opacity[Pixels::kReach][Pixels::kReach + 9] > 0);
}

void clipped_ribbon_matches_reference() {
	const std::vector<radar_point_t> path{
		{0, 0}, {-13, -8}, {-4, 9}, {-10, -5}, {6, 2}};
	const auto parts = tail(path);
	constexpr int left = -11, top = -5, width = 15, height = 11;
	constexpr std::ptrdiff_t stride = 19;
	std::array<uint8_t, height * stride> mask{};
	for (int y = 0; y < height; y++) {
		std::fill_n(mask.data() + y * stride + width, stride - width, 0xa5);
	}
	auto expected = mask;
	const Pixels reference = pixels(path);
	for (int y = 0; y < height; y++) {
		const auto* row = reference.opacity[Pixels::kReach + top + y];
		std::copy_n(row + Pixels::kReach + left, width,
					expected.data() + y * stride);
	}

	radar_track_ribbon(mask.data(), stride, left, top, left + width - 1,
					   top + height - 1, parts.data(),
					   static_cast<unsigned>(parts.size()));
	CHECK(mask == expected);
}
}

int main() {
	return check::run({
		TEST(dot_counts),
		TEST(dot_places),
		TEST(fixed_dot),
		TEST(tail_length),
		TEST(tail_shape),
		TEST(tail_pixels),
		TEST(tail_reversal_pixels),
		TEST(clipped_ribbon_matches_reference),
	});
}
