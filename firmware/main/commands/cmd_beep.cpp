#include "console.h"
#include "buzzer/buzzer.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "argtable.hpp"
#include "esp_console.h"
#include "command_status.hpp"
#include "esp_log.h"
#include "loop.hpp"

namespace {
	struct {
		struct arg_lit* schemes[5];
		struct arg_str* value;
		struct arg_end* end;
	} args;

	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (argc == 1) {
			const auto state = buzzer_state();
			static const char* mute[] = {"UNKNOWN", "ON", "OFF"};
			std::printf("%-20s %s\n%-20s %s\n%-20s", "Mute", mute[state.mute],
						"Playing", state.playing ? "YES" : "NO", "Schemes");
			for (size_t i = 0; const auto* scheme = buzzer_scheme_at(i); i++) std::printf(
				" %s", scheme->name);
			std::puts("");
			return 0;
		}
		unsigned selected = 0;
		const char* name = nullptr;
		for (size_t i = 0; i < 5; i++)
			if (args.schemes[i]->count) {
				selected++;
				name = buzzer_scheme_at(i)->name;
			}
		const char* value = args.value->count ? args.value->sval[0] : nullptr;
		bool off = value && !std::strcmp(value, "off");
		uint32_t hz = 0;
		bool valid = off ? selected == 0 : selected == 1;
		if (valid && value && !off) {
			char* end;
			errno = 0;
			unsigned long parsed = std::strtoul(value, &end, 10);
			valid = value[0] != '-' && !errno && end != value && !*end && parsed && parsed
				<= UINT32_MAX;
			hz = parsed;
		}
		if (!valid) {
			std::puts("usage: beep [--short|--long|--double|--alarm|--sweep] [hz] | off");
			return 1;
		}
		bool queued = loop::hsubmit("beep command", [name, hz](loop::Handle, loop::Value&) {
			if (!name) buzzer_off();
			else if (esp_err_t result = buzzer_play(name, hz); result != ESP_OK)
				ESP_LOGW("beep", "%s failed: %s", name, esp_err_to_name(result));
			return loop::Result::done;
		});
		return command_status::ack(queued ? ESP_OK : ESP_ERR_NO_MEM);
	}
}

extern "C" esp_err_t console_register_beep(void) {
	for (size_t i = 0; i < 5; i++)
		args.schemes[i] = arg_lit0(nullptr, buzzer_scheme_at(i)->name,
									"Play this beep scheme");
	args.value = arg_str0(nullptr, nullptr, "hz|off",
						"Frequency in Hz, default 4000, ignored for sweep, or off");
	args.end = arg_end(3);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "beep",
		.help = "Show mute and schemes, play a beep, or stop playback.",
		.hint = "[--short|--long|--double|--alarm|--sweep] [hz] | off", .func = command,
		.argtable = &args, .func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
