#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "console.h"
#include "esp_console.h"
#include "hwtest.hpp"
#include "linenoise/linenoise.h"

namespace {
void list() {
	std::printf("  #  %-20s %-9s %s\n", "Name", "Kind", "What it does");
	size_t number = 1;
	for (const hwtest::Entry& entry : hwtest::entries()) {
		std::printf("%3u  %-20s %-9s %s\n", (unsigned)number++, entry.name,
					entry.benchmark ? "benchmark" : "check", entry.what);
	}
}

bool run_entry(const hwtest::Entry& entry) {
	switch (hwtest::run(entry, stdout)) {
		case hwtest::RunResult::done:
			return true;
		case hwtest::RunResult::busy:
			std::printf("hwtest: a run is going on\n");
			return false;
		case hwtest::RunResult::no_memory:
			std::printf("no memory for the task of %s\n", entry.name);
			return false;
	}
	return false;
}

bool run_selection(const char* asked) {
	auto entries = hwtest::entries();
	if (std::strcmp(asked, "all") == 0) {
		return std::ranges::all_of(entries, run_entry);
	}
	const auto named =
		std::ranges::find_if(entries, [asked](const auto& entry) {
			return std::strcmp(asked, entry.name) == 0;
		});
	char* end = nullptr;
	const unsigned long number =
		named != entries.end()
			? static_cast<unsigned long>(named - entries.begin()) + 1
			: std::strtoul(asked, &end, 10);
	if ((end && (end == asked || *end != '\0')) || number < 1 ||
		number > entries.size()) {
		std::printf("hwtest: %s is not a listed test\n", asked);
		return false;
	}
	return run_entry(entries[number - 1]);
}

int hwtest_command(int argc, char** argv) {
	if (argc > 2) {
		std::printf("usage: hwtest [NAME|NUMBER|all]\n");
		return 1;
	}
	if (argc == 2) {
		return run_selection(argv[1]) ? 0 : 1;
	}
	list();
	for (;;) {
		char* line = linenoise("hwtest, name, number or all, empty to leave> ");
		const bool leave = line == nullptr || line[0] == '\0';
		if (!leave) {
			run_selection(line);
		}
		linenoiseFree(line);
		if (leave) {
			return 0;
		}
	}
}
}

extern "C" esp_err_t console_register_hwtest(void) {
	const esp_console_cmd_t command = {
		.command = "hwtest",
		.help = "List the checks and benchmarks of the device and run them.",
		.hint = "[NAME|NUMBER|all]",
		.func = hwtest_command,
		.argtable = nullptr,
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&command);
}
