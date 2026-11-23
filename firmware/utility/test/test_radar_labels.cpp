#include <vector>

#include "check.hpp"
#include "widgets/radar/radar_labels.h"

namespace {
constexpr radar_field_t FIELD = {
	.center = {200, 200},
	.inner = 100,
	.outer = 176,
	.prefer_left_after_x_px = 100,
	.bounds = {0, 0, 399, 399},
	.ring_labels = {{180, 292, 220, 308}, {180, 368, 220, 384}}};
constexpr radar_spot_t SIZE = {40, 20};

enum Side {
	right,
	left,
	below,
	above,
	below_right,
	above_right,
	below_left,
	above_left
};

std::vector<radar_box_t> placed(const std::vector<radar_spot_t>& symbols) {
	std::vector<radar_spot_t> sizes(symbols.size(), SIZE);
	std::vector<radar_box_t> labels(symbols.size());
	radar_labels(&FIELD, symbols.data(), sizes.data(),
				 static_cast<uint32_t>(symbols.size()), labels.data());
	return labels;
}

Side side(radar_spot_t symbol, const radar_box_t& label) {
	const bool to_right = label.x1 > symbol.x;
	const bool to_left = label.x2 < symbol.x;
	if (label.y1 > symbol.y) {
		return to_right ? below_right : to_left ? below_left : below;
	}
	if (label.y2 < symbol.y) {
		return to_right ? above_right : to_left ? above_left : above;
	}
	return to_right ? right : left;
}

bool overlap(const radar_box_t& a, const radar_box_t& b) {
	return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2;
}

radar_box_t around(radar_spot_t spot, int32_t reach) {
	return {spot.x - reach, spot.y - reach, spot.x + reach, spot.y + reach};
}

Side side_of(radar_spot_t symbol) { return side(symbol, placed({symbol})[0]); }

void free_space() {
	const radar_spot_t symbol = {150, 150};
	const radar_box_t label = placed({symbol})[0];
	CHECK_EQUAL(150 + RADAR_LABEL_GAP_PX, label.x1);
	CHECK_EQUAL(label.x1 + SIZE.x - 1, label.x2);
	CHECK_EQUAL(140, label.y1);
	CHECK_EQUAL(159, label.y2);
	const int flip_x = FIELD.center.x + FIELD.prefer_left_after_x_px;
	CHECK(side_of({flip_x + 1, 10}) == left);
	CHECK(side_of({flip_x, 10}) == right);
}

void rings() {
	const radar_spot_t inner_top = {200, 100};
	CHECK(side_of(inner_top) == below);
	const radar_spot_t inner_left = {100, 200};
	CHECK(side_of(inner_left) == right);
	const radar_spot_t inner_northeast = {270, 130};
	CHECK(side_of(inner_northeast) == right);
	const radar_spot_t outer_top = {200, 24};
	CHECK(side_of(outer_top) == below);
	// Between the rings every side crosses one: the first that was tried
	// stays, which is the left this far right.
	CHECK(side_of({320, 200}) == left);
}

void neighbours() {
	const radar_spot_t left_symbol{140, 60}, right_symbol{170, 60};
	std::vector<radar_spot_t> symbols = {left_symbol, right_symbol};
	std::vector<radar_box_t> labels = placed(symbols);
	CHECK(side(symbols[0], labels[0]) != right);
	CHECK(side(symbols[1], labels[1]) == right);

	// Two above each other: on its right the label of the second would lie
	// on the label of the first and on nothing else.
	symbols = {{150, 150}, {150, 165}};
	labels = placed(symbols);
	CHECK(side(symbols[0], labels[0]) == right);
	CHECK(side(symbols[1], labels[1]) == below);

	CHECK(side_of({FIELD.center.x - 30, FIELD.center.y}) != right);
}

void dots() {
	const radar_spot_t ahead[] = {{90, 20}, {110, 20}};
	radar_field_t field = FIELD;
	field.dots = ahead;
	field.dot_count = 2;
	const radar_spot_t symbol = {60, 20};
	radar_box_t label;
	radar_labels(&field, &symbol, &SIZE, 1, &label);
	CHECK(side(symbol, label) == left);
	CHECK(side_of(symbol) == right);
}

void ring_labels() {
	// Left of the label of each ring and clear of the ring itself: on the
	// right only the ring label is in the way.
	CHECK(side_of({150, 318}) == left);
	CHECK(side_of({150, 356}) == above);
}

void edge() {
	const radar_spot_t top_edge = {30, 5};
	CHECK(side_of(top_edge) == below);
	const radar_spot_t bottom_right_corner = {390, 395};
	const radar_box_t label = placed({bottom_right_corner})[0];
	CHECK(side(bottom_right_corner, label) == above_left);
	CHECK_EQUAL(336, label.x1);
	CHECK_EQUAL(361, label.y1);
}

void covers_nothing() {
	const std::vector<radar_spot_t> symbols = {
		{150, 150}, {150, 165}, {170, 200}, {150, 318},
		{30, 5},    {390, 395}, {60, 100}};
	const std::vector<radar_spot_t> dots = {{90, 100}, {110, 100}};
	radar_field_t field = FIELD;
	field.dots = dots.data();
	field.dot_count = static_cast<uint32_t>(dots.size());
	std::vector<radar_spot_t> sizes(symbols.size(), SIZE);
	std::vector<radar_box_t> labels(symbols.size());
	radar_labels(&field, symbols.data(), sizes.data(),
				 static_cast<uint32_t>(symbols.size()), labels.data());
	for (size_t i = 0; i < labels.size(); i++) {
		const radar_box_t& label = labels[i];
		CHECK(label.x1 >= FIELD.bounds.x1 && label.y1 >= FIELD.bounds.y1 &&
			  label.x2 <= FIELD.bounds.x2 && label.y2 <= FIELD.bounds.y2);
		CHECK(!overlap(label, around(FIELD.center, RADAR_SYMBOL_RADIUS_PX)));
		for (size_t other = 0; other < labels.size(); other++) {
			if (other != i) {
				CHECK(!overlap(label, labels[other]));
				CHECK(!overlap(label,
							   around(symbols[other], RADAR_SYMBOL_RADIUS_PX)));
			}
		}
		for (const radar_spot_t& dot : dots) {
			CHECK(!overlap(label, around(dot, RADAR_DOT_RADIUS_PX)));
		}
		for (const radar_box_t& ring_label : FIELD.ring_labels) {
			CHECK(!overlap(label, ring_label));
		}
	}
}
}

int main() {
	return check::run({
		TEST(free_space),
		TEST(rings),
		TEST(neighbours),
		TEST(dots),
		TEST(ring_labels),
		TEST(edge),
		TEST(covers_nothing),
	});
}
