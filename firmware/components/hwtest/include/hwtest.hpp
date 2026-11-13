#pragma once

// Hardware test mode: checks read the driver States and print one line per
// check, "HWTEST <name> PASS|FAIL <detail>", for a host script to judge.

namespace hwtest {
void report(const char* name, bool pass, const char* detail);

// Runs the checks on the calling task and blocks up to 6 s for the drivers to
// get ready. The loop must be started and the drivers initialised.
void run();
}  // namespace hwtest
