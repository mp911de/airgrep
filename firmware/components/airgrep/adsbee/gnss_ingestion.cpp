#include "gnss_ingestion.hpp"

#include "esp_timer.h"
#include "events/events.hpp"
#include "subscribers.hpp"

namespace adsbee {
namespace {
Subscribers<GnssHandler, 4> gnss_subscribers;
bool fix_held;  // loop task only
}

void publish_fix(bool valid, AntennaStatus antenna) {
	if (events::update_if_changed(fix_held, valid)) {
		events::publish(events::Event::fix_changed(valid, {antenna}));
	}
}

void expire_fix(int64_t now_us) {
	if (!fix_held) {
		return;
	}
	bool valid = false;
	AntennaStatus antenna = AntennaStatus::unreported;
	State::update([&](const State& state) {
		valid = state.gnss_at(now_us).fix != Fix::none;
		antenna = state.gnss.antenna;
	});
	publish_fix(valid, antenna);
}

bool subscribe_gnss(GnssHandler handler, void* context) {
	return gnss_subscribers.add(handler, context);
}

void ingest_gnss(const Gnss& gnss, int64_t received_us) {
	expire_fix(received_us);
	State::update([&](State& state) {
		state.gnss = gnss;
		state.last_gnss_report_us = received_us;
	});
	if (!state_usable(received_us, esp_timer_get_time())) {
		publish_fix(false, gnss.antenna);
		return;
	}
	const bool has_fix = gnss.fix != Fix::none;
	gnss_subscribers.notify(gnss, received_us, has_fix && !fix_held);
	publish_fix(has_fix, gnss.antenna);
}

void reset_ingestion() {
	State::update([](State& state) {
		state.traffic_ingestion = Ingestion::read;
		state.gnss_ingestion = Ingestion::read;
	});
}
}
