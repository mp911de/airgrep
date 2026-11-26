#include "zones.hpp"

#include <dlfcn.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern "C" {
#include "tz/tz_lookup.h"
}

namespace remote::zones {
namespace {
constexpr int32_t kDay = 86400, kMicrodegrees = 1'000'000;
constexpr int64_t kDayMinutes = 1440;

// One of the two changes of a POSIX rule: Mm.w.d, Jn or n, and the local
// time of day.
struct Change {
	enum class Form { weekday_of_month, day_without_leap_day, day } form{};
	unsigned month = 0, week = 0, weekday = 0;
	int day = 0;
	int32_t at_s = 7200;
};

struct Rule {
	std::string standard = "UTC", daylight;
	int32_t standard_east_s = 0, daylight_east_s = 0;
	Change begins, ends;
};

struct Reader {
	std::string_view left;

	bool take(char wanted) {
		if (left.empty() || left.front() != wanted) {
			return false;
		}
		left.remove_prefix(1);
		return true;
	}
	int number() {
		int value = 0;
		while (!left.empty() && left.front() >= '0' && left.front() <= '9') {
			value = value * 10 + (left.front() - '0');
			left.remove_prefix(1);
		}
		return value;
	}
	std::string name() {
		const bool quoted = take('<');
		size_t length = 0;
		while (length < left.size() &&
			   (quoted ? left[length] != '>'
					   : std::isalpha(
							 static_cast<unsigned char>(left[length])) != 0)) {
			length++;
		}
		std::string text(left.substr(0, length));
		left.remove_prefix(length);
		(void)take('>');
		return text;
	}
	// [+-]h[:m[:s]] in seconds.
	int32_t duration() {
		const bool negative = take('-');
		if (!negative) {
			(void)take('+');
		}
		int32_t seconds = number() * 3600;
		if (take(':')) {
			seconds += number() * 60;
			if (take(':')) {
				seconds += number();
			}
		}
		return negative ? -seconds : seconds;
	}
	Change change() {
		Change read;
		if (take('M')) {
			read.form = Change::Form::weekday_of_month;
			read.month = static_cast<unsigned>(number());
			(void)take('.');
			read.week = static_cast<unsigned>(number());
			(void)take('.');
			read.weekday = static_cast<unsigned>(number());
		} else {
			read.form = take('J') ? Change::Form::day_without_leap_day
								  : Change::Form::day;
			read.day = number();
		}
		if (take('/')) {
			read.at_s = duration();
		}
		return read;
	}
};

// POSIX counts an offset west of Greenwich, and daylight time is an hour
// ahead unless the rule says otherwise.
Rule parse(std::string_view text) {
	Rule rule;
	if (text.empty()) {
		return rule;
	}
	Reader reader{text};
	rule.standard = reader.name();
	rule.standard_east_s = -reader.duration();
	if (reader.left.empty()) {
		return rule;
	}
	rule.daylight = reader.name();
	rule.daylight_east_s = reader.left.empty() || reader.left.front() == ','
							   ? rule.standard_east_s + 3600
							   : -reader.duration();
	(void)reader.take(',');
	rule.begins = reader.change();
	(void)reader.take(',');
	rule.ends = reader.change();
	return rule;
}

int64_t day_of(const Change& change, std::chrono::year year) {
	using namespace std::chrono;
	const sys_days first_of_year = year / January / 1;
	switch (change.form) {
		case Change::Form::weekday_of_month: {
			const year_month month = year / std::chrono::month{change.month};
			const weekday wanted{change.weekday};
			const sys_days day = change.week == 5
									 ? sys_days{month / wanted[last]}
									 : sys_days{month / wanted[change.week]};
			return day.time_since_epoch().count();
		}
		case Change::Form::day_without_leap_day:
			return (first_of_year +
					days{change.day - 1 +
						 (year.is_leap() && change.day >= 60 ? 1 : 0)})
				.time_since_epoch()
				.count();
		case Change::Form::day:
			return (first_of_year + days{change.day})
				.time_since_epoch()
				.count();
	}
	return 0;
}

bool daylight(const Rule& rule, int64_t utc_s) {
	if (rule.daylight.empty()) {
		return false;
	}
	using namespace std::chrono;
	const year year =
		year_month_day{
			floor<days>(sys_seconds{seconds{utc_s + rule.standard_east_s}})}
			.year();
	const int64_t begins = day_of(rule.begins, year) * kDay + rule.begins.at_s -
						   rule.standard_east_s;
	const int64_t ends =
		day_of(rule.ends, year) * kDay + rule.ends.at_s - rule.daylight_east_s;
	return begins < ends ? utc_s >= begins && utc_s < ends
						 : utc_s >= begins || utc_s < ends;
}

int32_t east_s(const Rule& rule, int64_t utc_s) {
	return daylight(rule, utc_s) ? rule.daylight_east_s : rule.standard_east_s;
}

struct Table {
	std::vector<char> blob;
	tz_table lookup{};
	bool mounted = false;
	// By zone number. Number 0 is no zone.
	std::vector<Rule> rules;
	std::unordered_map<std::string_view, size_t> by_name;
	std::optional<int64_t> listed_at_s;
	std::vector<display::Zone> listed;
};

// The table lies in the application beside the library that holds this code.
std::filesystem::path table_path() {
	Dl_info library;
	if (dladdr(reinterpret_cast<const void*>(&at), &library) == 0 ||
		!library.dli_fname) {
		return {};
	}
	return std::filesystem::path(library.dli_fname)
			   .parent_path()
			   .parent_path() /
		   "Resources" / "tzdata.bin";
}

Table& table() {
	static Table loaded;
	static const bool initialized = [] {
		std::ifstream file(table_path(), std::ios::binary);
		loaded.blob.assign(std::istreambuf_iterator<char>(file), {});
		loaded.mounted =
			tz_init(&loaded.lookup, loaded.blob.data(),
					static_cast<uint32_t>(loaded.blob.size())) != 0;
		const uint16_t count =
			loaded.mounted ? tz_zone_count(&loaded.lookup) : 0;
		loaded.rules.reserve(count);
		loaded.by_name.reserve(count);
		for (uint16_t zone = 0; zone < count; zone++) {
			loaded.rules.push_back(parse(tz_posix(&loaded.lookup, zone)));
			if (zone > 0) {
				loaded.by_name.emplace(tz_name(&loaded.lookup, zone), zone);
			}
		}
		return true;
	}();
	(void)initialized;
	return loaded;
}

const Rule* rule_of(const char* zone) {
	if (!zone || !zone[0]) {
		return nullptr;
	}
	const Table& known = table();
	const auto found = known.by_name.find(zone);
	return found == known.by_name.end() ? nullptr : &known.rules[found->second];
}
}

std::span<const display::Zone> listed(int64_t utc_s) {
	Table& known = table();
	if (known.listed_at_s == utc_s) {
		return known.listed;
	}
	known.listed_at_s = utc_s;
	known.listed.clear();
	known.listed.reserve(known.rules.size() > 1 ? known.rules.size() - 1 : 0);
	for (size_t number = 1; number < known.rules.size(); number++) {
		known.listed.push_back(
			{tz_name(&known.lookup, static_cast<uint16_t>(number)),
			 static_cast<int16_t>(east_s(known.rules[number], utc_s) / 60)});
	}
	return known.listed;
}

const char* at(double latitude, double longitude) {
	const Table& known = table();
	if (!known.mounted || !std::isfinite(latitude) ||
		!std::isfinite(longitude) || latitude < -90.0 || latitude > 90.0 ||
		longitude < -180.0 || longitude > 180.0) {
		return "";
	}
	const uint16_t zone =
		tz_lookup(&known.lookup,
				  static_cast<int32_t>(std::lround(latitude * kMicrodegrees)),
				  static_cast<int32_t>(std::lround(longitude * kMicrodegrees)));
	return zone == TZ_INVALID || zone == 0 ? "" : tz_name(&known.lookup, zone);
}

display::mapping::Clock local(const char* zone, int64_t utc_s) {
	static const Rule utc;
	const Rule* found = rule_of(zone);
	const Rule& rule = found ? *found : utc;
	const bool dst = daylight(rule, utc_s);
	const int32_t offset_s = dst ? rule.daylight_east_s : rule.standard_east_s;
	const int64_t minutes =
		((utc_s + offset_s) / 60 % kDayMinutes + kDayMinutes) % kDayMinutes;
	return {static_cast<uint8_t>(minutes / 60),
			static_cast<uint8_t>(minutes % 60),
			dst ? rule.daylight.c_str() : rule.standard.c_str()};
}
}
