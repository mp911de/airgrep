#include <cmath>
#include <limits>
#include <vector>

#include "check.hpp"
#include "conflicts/conflicts.hpp"
#include "conflicts/traffic_filter.hpp"
#include "navigation/units.h"

// The conflict engine of the display against encounters whose outcome is not
// in doubt. The thresholds are those of DO-365B and are not asserted here:
// each case is far on one side of them.

namespace {
using conflicts::Conflict;
using conflicts::Flight;
using conflicts::Level;

Flight own_ship() {
	return {.id = 0,
			.latitude_deg = 53.4,
			.longitude_deg = 7.19,
			.altitude_ft = 3000,
			.true_track_deg = 0,
			.groundspeed_kt = 100};
}

Flight flight_at_bearing(float range_nm, float bearing_deg, float track_deg,
						 float speed_kt, float above_ft = 0) {
	Flight own = own_ship(), flight = own;
	const double bearing = bearing_deg * units::radians_per_degree<double>;
	flight.id = 1;
	flight.latitude_deg +=
		range_nm * std::cos(bearing) / units::nm_per_degree_latitude;
	flight.longitude_deg +=
		range_nm * std::sin(bearing) / units::nm_per_degree_latitude /
		std::cos(own.latitude_deg * units::radians_per_degree<double>);
	flight.altitude_ft += above_ft;
	flight.true_track_deg = track_deg;
	flight.groundspeed_kt = speed_kt;
	return flight;
}

Flight advance_flight(Flight flight, double seconds) {
	const double course =
		flight.true_track_deg * units::radians_per_degree<double>;
	const double nm = flight.groundspeed_kt * seconds / 3600;
	flight.latitude_deg +=
		nm * std::cos(course) / units::nm_per_degree_latitude;
	flight.longitude_deg +=
		nm * std::sin(course) / units::nm_per_degree_latitude /
		std::cos(flight.latitude_deg * units::radians_per_degree<double>);
	flight.altitude_ft +=
		static_cast<float>(flight.vertical_fpm * seconds / 60);
	flight.time_s += seconds;
	return flight;
}

Conflict detect_over_seconds(conflicts::ConflictEngine& engine, Flight target,
							 int seconds = 5) {
	Flight own = own_ship();
	Conflict found;
	for (int second = 0; second <= seconds; second++) {
		engine.detect(advance_flight(own, second),
					  {{advance_flight(target, second)}}, {&found, 1});
	}
	return found;
}

void head_on() {
	// 2 NM ahead and coming: 36 s to the meeting.
	conflicts::ConflictEngine engine;
	Conflict found =
		detect_over_seconds(engine, flight_at_bearing(2, 0, 180, 100));
	CHECK(found.level == Level::alert);
	CHECK(found.within_s < 30);
}

void far_away() {
	conflicts::ConflictEngine engine;
	Conflict found =
		detect_over_seconds(engine, flight_at_bearing(15, 0, 180, 100));
	CHECK(found.level == Level::other);
	CHECK_EQUAL(conflicts::NO_CONFLICT, found.within_s);
}

void above() {
	conflicts::ConflictEngine engine;
	CHECK(detect_over_seconds(engine, flight_at_bearing(2, 0, 180, 100, 3000))
			  .level == Level::other);
}

void receding_abeam_does_not_alert() {
	conflicts::ConflictEngine engine;
	CHECK(detect_over_seconds(engine, flight_at_bearing(1.5f, 90, 90, 100))
			  .level == Level::other);
}

void crossing() {
	// From the right, to where the own ship is 72 s from now.
	conflicts::ConflictEngine engine;
	Conflict found =
		detect_over_seconds(engine, flight_at_bearing(2.83f, 45, 270, 100));
	CHECK(found.level != Level::other);
	CHECK(found.within_s < 72);
}

void closing_in() {
	conflicts::ConflictEngine engine;
	Conflict early =
		detect_over_seconds(engine, flight_at_bearing(3, 0, 180, 100), 5);
	conflicts::ConflictEngine again;
	Conflict late =
		detect_over_seconds(again, flight_at_bearing(3, 0, 180, 100), 15);
	CHECK(early.level != Level::other);
	CHECK(late.within_s < early.within_s);
}

void coming() {
	conflicts::ConflictEngine engine;
	const Flight closing_head_on = flight_at_bearing(6, 0, 180, 100);
	const Flight receding = flight_at_bearing(6, 0, 0, 150);
	Conflict found = detect_over_seconds(engine, closing_head_on);
	CHECK(found.level == Level::other);
	CHECK(found.enters_corrective_volume);
	CHECK(!detect_over_seconds(engine, receding).enters_corrective_volume);
}

void standing() {
	conflicts::ConflictEngine engine;
	const Flight hovering_ahead = flight_at_bearing(3, 0, 0, 0);
	const Flight hovering_abeam = flight_at_bearing(1.5f, 90, 0, 0);
	CHECK(detect_over_seconds(engine, hovering_ahead).enters_corrective_volume);
	CHECK(
		!detect_over_seconds(engine, hovering_abeam).enters_corrective_volume);
}

void two_targets() {
	conflicts::ConflictEngine engine;
	Flight own = own_ship(), coming = flight_at_bearing(2, 0, 180, 100),
		   far = flight_at_bearing(15, 180, 180, 100);
	far.id = 2;
	Conflict found[2];
	for (int second = 0; second <= 5; second++) {
		std::vector<Flight> traffic{advance_flight(far, second),
									advance_flight(coming, second)};
		engine.detect(advance_flight(own, second), traffic, found);
	}
	CHECK(found[0].level == Level::other);
	CHECK(found[1].level == Level::alert);
}

void nonsense() {
	conflicts::ConflictEngine engine;
	Flight broken = flight_at_bearing(2, 0, 180, 100);
	broken.latitude_deg = std::numeric_limits<double>::quiet_NaN();
	std::array<uint16_t, 1> order{};
	CHECK_EQUAL(0u, conflicts::filter_traffic(own_ship(), std::span(&broken, 1),
											  order));
	CHECK(detect_over_seconds(engine, broken).level == Level::other);
	CHECK(
		detect_over_seconds(engine, flight_at_bearing(2, 0, 180, 100)).level ==
		Level::alert);
}

void departed_aircraft() {
	conflicts::ConflictEngine engine;
	Flight own = own_ship(), close = flight_at_bearing(2, 0, 180, 100);
	constexpr uint32_t retained_id = 1, returning_id = 2;
	Flight flights[] = {close, close};
	flights[0].id = retained_id;
	flights[1].id = returning_id;
	Conflict found[2];
	engine.detect(own, flights, found);
	CHECK(found[0].level == Level::alert);
	CHECK(found[1].level == Level::alert);

	Flight far = flight_at_bearing(8, 180, 180, 100);
	far.id = retained_id;
	flights[0] = advance_flight(far, 1);
	engine.detect(advance_flight(own, 1), {flights, 1}, found);
	CHECK(found[0].level == Level::alert);
	flights[0] = flights[1] = advance_flight(far, 2);
	flights[1].id = returning_id;
	engine.detect(advance_flight(own, 2), flights, found);
	CHECK(found[0].level == Level::alert);
	CHECK(found[1].level == Level::other);
}

void within_reach() {
	// Own ship and flight close at 250 kt at the most: 12.5 NM in the three
	// minutes the engine looks ahead.
	Flight own = own_ship();
	Flight traffic[] = {flight_at_bearing(30, 0, 180, 100),
						flight_at_bearing(10, 90, 0, 100),
						flight_at_bearing(2, 0, 180, 100, 9000),
						flight_at_bearing(2, 180, 0, 100)};
	uint16_t order[4];
	CHECK_EQUAL(2u, conflicts::filter_traffic(own, traffic, order));
	CHECK_EQUAL(3, order[0]);
	CHECK_EQUAL(1, order[1]);
	CHECK_EQUAL(1u, conflicts::filter_traffic(own, traffic, {order, 1}));
	CHECK_EQUAL(3, order[0]);
	traffic[0].groundspeed_kt = 500;
	CHECK_EQUAL(3u, conflicts::filter_traffic(own, traffic, order));
	CHECK_EQUAL(0, order[2]);
}

void urgent_encounter_survives_candidate_limit() {
	conflicts::ConflictEngine engine;
	Flight own = own_ship();
	std::vector<Flight> traffic;
	for (size_t i = 0; i < conflicts::kMaxAlertTargets + 20; i++) {
		traffic.push_back(
			flight_at_bearing(4 + 0.05f * static_cast<float>(i), 90, 0, 100));
		traffic.back().id = 10 + i;
	}
	const Flight urgent_head_on = flight_at_bearing(2, 0, 180, 100);
	traffic.push_back(urgent_head_on);
	std::vector<Conflict> found(traffic.size());
	for (int second = 0; second <= 5; second++) {
		std::vector<Flight> now;
		now.reserve(traffic.size());
		for (const Flight& flight : traffic) {
			now.push_back(advance_flight(flight, second));
		}
		engine.detect(advance_flight(own, second), now, found);
	}
	CHECK(found.back().level == Level::alert);
	CHECK(found.front().level == Level::other);
	conflicts::Band bands[32];
	const size_t count = engine.bands(conflicts::Guide::track, bands);
	CHECK(count > 0);
	conflicts::Band first[2]{};
	first[1].from = -1234;
	CHECK_EQUAL(
		1u, engine.bands(conflicts::Guide::track, std::span(first).first(1)));
	CHECK(first[0].region == bands[0].region);
	CHECK_EQUAL(bands[0].from, first[0].from);
	CHECK_EQUAL(-1234, first[1].from);
	CHECK_EQUAL(0u, engine.bands(conflicts::Guide::track, {}));

	bool ahead = false;
	for (size_t i = 0; i < count; i++) {
		ahead |= bands[i].region == conflicts::BandRegion::warning &&
				 bands[i].from <= own.true_track_deg &&
				 own.true_track_deg <= bands[i].to;
	}
	CHECK(ahead);
}
}

int main() {
	return check::run({
		TEST(head_on),
		TEST(far_away),
		TEST(above),
		TEST(receding_abeam_does_not_alert),
		TEST(crossing),
		TEST(closing_in),
		TEST(coming),
		TEST(standing),
		TEST(two_targets),
		TEST(departed_aircraft),
		TEST(nonsense),
		TEST(within_reach),
		TEST(urgent_encounter_survives_candidate_limit),
	});
}
