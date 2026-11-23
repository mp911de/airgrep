#pragma once

namespace wifi {
enum class Command { none, query, set };

// Only the latest slider position matters. A set takes priority over a query.
class Requests {
   public:
	void apply() { next_ = Command::set; }
	void poll() {
		if (next_ == Command::none) {
			next_ = Command::query;
		}
	}
	bool set_pending() const { return next_ == Command::set; }

	Command submit() {
		if (submitted_ != Command::none) {
			return Command::none;
		}
		submitted_ = next_;
		return submitted_;
	}

	void written(bool still_wanted) {
		if (submitted_ == Command::set && still_wanted) {
			next_ = Command::query;
		} else if (submitted_ == Command::query && next_ == Command::query) {
			next_ = Command::none;
		}
	}

	// A rejection before the write leaves the request pending for the next
	// free slot. A missing reply after a write still leads to verification.
	void finished() { submitted_ = Command::none; }

   private:
	Command next_ = Command::none;
	Command submitted_ = Command::none;
};
}
