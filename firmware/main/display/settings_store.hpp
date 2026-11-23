#pragma once

#include <cstddef>

namespace display {
struct Storage {
	bool (*read)(const char* key, void* out, size_t size);
	bool (*write)(const char* key, const void* blob, size_t size);
};

template <typename Settings>
class SettingsStore {
   public:
	constexpr SettingsStore(const char* key, Storage storage) noexcept
		: key_(key), storage_(storage) {}

	void load() {
		Settings stored;
		settings_ = storage_.read(key_, &stored, sizeof stored) && valid(stored)
						? stored
						: Settings{};
	}

	bool save(const Settings& settings) {
		if (!valid(settings) ||
			!storage_.write(key_, &settings, sizeof settings)) {
			return false;
		}
		settings_ = settings;
		return true;
	}

	const Settings& settings() const { return settings_; }

   private:
	const char* key_;
	Storage storage_;
	Settings settings_{};
};
}
