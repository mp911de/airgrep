#include "console.h"
#include <cstdio>
#include <cstring>
#include "argtable.hpp"
#include "esp_console.h"
#include "settings/settings.hpp"
#include "command_status.hpp"

namespace {
	struct {
		struct arg_lit* reset;
		struct arg_lit* format;
		struct arg_str* field;
		struct arg_str* value;
		struct arg_end* end;
	} args;

	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (args.reset->count + args.format->count + (args.field->count ? 1 : 0) > 1 ||
			(args.value->count && !args.field->count)) {
			std::puts("usage: settings [<field> <value>] | --reset | --format");
			return 1;
		}
		if (args.reset->count) return command_status::ack(settings::reset());
		if (args.format->count) return command_status::ack(settings::format());
		if (args.value->count) return command_status::ack(settings::set(args.field->sval[0], args.value->sval[0]));
		const settings::Settings current = settings::copy();
		const settings::Settings fallback = settings::defaults();
		const settings::Status status = settings::status();
		size_t count;
		const settings::Field* fields = settings::fields(count);
		char value[SETTINGS_TZ_RULE_SIZE], standard[SETTINGS_TZ_RULE_SIZE];
		for (size_t i = 0; i < count; i++) {
			if (args.field->count && std::strcmp(fields[i].name, args.field->sval[0]) != 0) continue;
			settings::format(current, fields[i], value, sizeof value);
			settings::format(fallback, fields[i], standard, sizeof standard);
			std::printf("%-20s %s (default %s)\n", fields[i].name, value, standard);
		}
		if (args.field->count && !settings::find(args.field->sval[0])) {
			std::printf("FAIL unknown field %s\n", args.field->sval[0]);
			return 1;
		}
		std::printf("%-20s version %u, %s\n", "Storage", status.stored_version,
			status.loaded ? "LOADED" : "DEFAULTS");
		if (status.last_error != ESP_OK) std::printf("%-20s %s\n", "Last error", esp_err_to_name(status.last_error));
		return 0;
	}
}

extern "C" esp_err_t console_register_settings(void) {
	args.reset = arg_lit0(nullptr, "reset", "Restore every setting to its default and save");
	args.format = arg_lit0(nullptr, "format", "Erase the NVS partition and start from defaults");
	args.field = arg_str0(nullptr, nullptr, "field", "Setting to show or set");
	args.value = arg_str0(nullptr, nullptr, "value", "New value, saved at once");
	args.end = arg_end(3);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "settings", .help = "List, set or reset the persistent settings.",
		.hint = "[<field> <value>] | --reset | --format", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
