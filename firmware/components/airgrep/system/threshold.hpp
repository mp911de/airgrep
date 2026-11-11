#pragma once

// A limit with a release point across a gap. A limit above its release point
// trips at or above the limit, one below trips at or below it. Between the
// two points the last state holds.
class Threshold {
   public:
	constexpr Threshold(float limit, float release)
		: limit_(limit), release_(release) {}

	bool update(float value) {
		const bool rising = limit_ > release_;
		if (rising ? value >= limit_ : value <= limit_) {
			tripped_ = true;
		} else if (rising ? value <= release_ : value >= release_) {
			tripped_ = false;
		}
		return tripped_;
	}

	bool tripped() const { return tripped_; }

   private:
	float limit_, release_;
	bool tripped_ = false;
};
