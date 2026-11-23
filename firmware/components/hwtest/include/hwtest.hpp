#pragma once

#include <cstdio>
#include <span>

namespace hwtest {
class Output {
   public:
	explicit Output(std::FILE* stream) : stream_(stream) {}
	void print(const char* format, ...) __attribute__((format(printf, 2, 3)));
	// One line "HWTEST <name> PASS|FAIL <detail>", for a host script to judge.
	void report(const char* name, bool pass, const char* detail);

   private:
	std::FILE* stream_;
};

struct Entry {
	const char* name;
	const char* what;
	bool benchmark;
	void (*run)(Output& output);
};

std::span<const Entry> entries();

enum class RunResult { done, busy, no_memory };
RunResult run(const Entry& entry, std::FILE* stream);

// Call after starting the loop and initializing the drivers.
void run_boot_checks();
}
