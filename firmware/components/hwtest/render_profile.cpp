#include "render_profile.hpp"

#include <algorithm>

#include "render_profile_hooks.h"

namespace {
// The headless display draws synchronously on the sole hwtest task.
hwtest::render::DrawProfile* active;
}

extern "C" void hwtest_draw_begin(const char* name) {
	if (active) {
		active->enter(name);
	}
}

extern "C" void hwtest_draw_end(const char* name) {
	if (active) {
		active->leave(name);
	}
}

namespace hwtest::render {
void DrawProfile::start() { active = this; }

void DrawProfile::stop() { active = nullptr; }

void DrawProfile::enter(const char* name) {
	if (invalid_) {
		return;
	}
	const auto now = Clock::now();
	if (depth_) {
		stack_[depth_ - 1]->self += now - last_;
	}
	last_ = now;
	auto end = operations_.begin() + used_;
	auto op = std::find_if(operations_.begin(), end, [name](const auto& row) {
		return row.name == name;
	});
	if (op == operations_.end() || depth_ == stack_.size()) {
		invalid_ = true;
		return;
	}
	if (op == end) {
		op->name = name;
		++used_;
	}
	++op->calls;
	stack_[depth_++] = &*op;
}

void DrawProfile::leave(const char* name) {
	if (invalid_ || !depth_ || stack_[depth_ - 1]->name != name) {
		invalid_ = true;
		return;
	}
	const auto now = Clock::now();
	stack_[--depth_]->self += now - last_;
	last_ = now;
}

bool DrawProfile::print(Output& output, Clock::duration draw) {
	if (invalid_ || depth_ || !used_) {
		output.print(
			"Incomplete LVGL draw profile (operations %u, open scopes %u)\n",
			(unsigned)used_, (unsigned)depth_);
		return false;
	}
	auto end = operations_.begin() + used_;
	std::sort(operations_.begin(), end,
			  [](const auto& a, const auto& b) { return a.self > b.self; });
	auto remaining = draw;
	using Milliseconds = std::chrono::duration<double, std::milli>;
	for (auto it = operations_.begin(); it != end; ++it) {
		output.print("  %7.2f ms self  %6u calls  %s\n",
					 Milliseconds(it->self).count(), it->calls, it->name);
		remaining -= it->self;
	}
	output.print("  %7.2f ms outside draw hooks\n",
				 Milliseconds(remaining).count());
	return true;
}
}
