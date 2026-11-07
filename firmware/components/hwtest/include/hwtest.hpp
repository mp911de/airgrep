#pragma once

// Hardware test mode: checks run as actions on the loop and print one line
// per check, "HWTEST <name> PASS|FAIL <detail>", for a host script to judge.

namespace hwtest {
	void report(const char* name, bool pass, const char* detail);

	// Submits the checks. The loop must be started and the drivers initialised.
	void run();
} // namespace hwtest
