#include "dump.hpp"

#include <format>

#include "inspect.hpp"

namespace device {
namespace {
constexpr size_t kBytesPerLine = 16;

void heading(std::string& text, const std::string& title) {
	text += std::format("\n== {}\n", title);
}

void hex(std::string& text, std::span<const uint8_t> bytes) {
	for (size_t at = 0; at < bytes.size(); at += kBytesPerLine) {
		text += std::format("  {:04x} ", at);
		for (uint8_t byte :
			 bytes.subspan(at, std::min(kBytesPerLine, bytes.size() - at))) {
			text += std::format(" {:02x}", byte);
		}
		text += '\n';
	}
}

const char* name(adsbee::Fix fix) {
	switch (fix) {
		case adsbee::Fix::fix_3d:
			return "3D";
		case adsbee::Fix::fix_2d:
			return "2D";
		default:
			return "none";
	}
}

const char* name(traffic::Sample sample) {
	switch (sample) {
		case traffic::Sample::fresh:
			return "fresh";
		case traffic::Sample::held:
			return "held";
		default:
			return "none";
	}
}

void target(std::string& text, const traffic::Tracked& tracked,
			const adsbee::Gnss& own) {
	const traffic::Target& report = tracked.target;
	text += std::format("{:<8} {:06X} {}  seen at {:.1f} s\n",
						report.callsign[0] ? report.callsign : "-",
						report.address, traffic::source_name(report.source),
						static_cast<double>(report.last_seen_us) / 1e6);
	if (report.position_valid) {
		text += std::format("  position {:.5f} {:.5f}\n", report.latitude,
							report.longitude);
	} else {
		text += "  position unknown\n";
	}
	float distance = 0, bearing = 0;
	if (own.fix != adsbee::Fix::none &&
		traffic::relative_position(report, own.latitude, own.longitude,
								   distance, bearing)) {
		text += std::format("  from own ship {:.2f} NM at {:05.1f} true\n",
							distance, bearing);
	} else {
		text += "  from own ship unknown\n";
	}
	text += std::format(
		"  altitude baro {} gnss {}  speed {}  track {}  rate baro {} gnss "
		"{}\n",
		report.baro_alt_valid ? std::format("{} ft", report.baro_alt_ft) : "-",
		report.gnss_alt_valid ? std::format("{} ft", report.gnss_alt_ft) : "-",
		report.speed_valid ? std::format("{} kt", report.speed_kt) : "-",
		report.direction_valid ? std::format("{:.1f}", report.track_deg) : "-",
		report.baro_rate_valid ? std::format("{} fpm", report.baro_rate_fpm)
							   : "-",
		report.gnss_rate_valid ? std::format("{} fpm", report.gnss_rate_fpm)
							   : "-");
	for (size_t age = 0; age < traffic::kHistorySeconds; age++) {
		const traffic::Position& was = tracked.past[age];
		if (was.sample != traffic::Sample::none) {
			const auto altitude =
				was.altitude_datum == traffic::AltitudeDatum::unavailable
					? "-"
					: std::format("{} ft", was.altitude_ft);
			text += std::format("  -{:02} s  {:.5f} {:.5f}  {}  {} kt  {}\n",
								age, was.latitude, was.longitude, altitude,
								was.speed_kt, name(was.sample));
		}
	}
}
}

std::string dump(const DeviceState& picture) {
	const adsbee::Gnss& own = picture.states.adsbee.gnss;
	std::string text = std::format(
		"== Device, {} s after its start\n== Own ship\nfix {}  position "
		"{:.5f} {:.5f}  altitude {} ft  speed {:.1f} kt  track {:.1f}\n",
		picture.hello.uptime_seconds, name(own.fix), own.latitude,
		own.longitude, own.altitude_ft, own.speed_kt, own.track_deg);

	heading(text, std::format("Traffic, {} listed of {} the device holds",
							  picture.traffic_listing.nearest_targets.size(),
							  picture.traffic_listing.total_targets));
	for (const traffic::Tracked& tracked :
		 picture.traffic_listing.nearest_targets) {
		target(text, tracked, own);
	}

	heading(text, "Fields, !! in front of one that cannot be right");
	text += inspect(picture, end_of_uptime_second_us(picture.hello)).text;

	heading(text, "Bytes");

	for (const Component component : picture.components) {
		const std::span<const uint8_t> bytes = picture.states.bytes(component);
		text += std::format("{}, {} bytes\n", name(component), bytes.size());
		hex(text, bytes);
	}
	for (const traffic::Tracked& tracked :
		 picture.traffic_listing.nearest_targets) {
		text += std::format("listed {:06X}, {} bytes\n", tracked.target.address,
							sizeof tracked.target);
		hex(text, bytes_of(tracked.target));
	}
	return text;
}
}
