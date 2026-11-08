#include "settings_model.hpp"
#include <cstring>

namespace settings {
	Settings defaults() {
		Settings result{};
		result.version = SETTINGS_VERSION;
		std::strncpy(result.tz_rule, SETTINGS_DEFAULT_TZ_RULE, sizeof result.tz_rule - 1);
		result.tz_detect = SETTINGS_DEFAULT_TZ_DETECT;
		return result;
	}

	bool migrate(const void* blob, size_t size, Settings& out) {
		if (!blob || size < sizeof(uint16_t)) return false;
		uint16_t version;
		std::memcpy(&version, blob, sizeof version);
		// Version 1 is the first layout. Each later version adds a case that converts
		// the previous struct here, oldest first, so a blob walks forward step by step.
		if (version == 1 && size == sizeof(Settings)) {
			std::memcpy(&out, blob, sizeof out);
			out.tz_rule[sizeof out.tz_rule - 1] = 0;
			return true;
		}
		return false;
	}

	namespace {
		const Field table[] = {
			{"tz_rule", Type::text, offsetof(Settings, tz_rule), SETTINGS_TZ_RULE_SIZE},
			{"tz_detect", Type::boolean, offsetof(Settings, tz_detect), 1},
		};

		bool equals(const char* text, size_t length, const char* word) {
			return std::strlen(word) == length && std::strncmp(text, word, length) == 0;
		}

		bool blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
	}

	const Field* fields(size_t& count) {
		count = sizeof table / sizeof table[0];
		return table;
	}

	const Field* find(const char* name) {
		for (const Field& field : table)
			if (std::strcmp(field.name, name) == 0) return &field;
		return nullptr;
	}

	bool assign(Settings& settings, const char* name, const char* value) {
		const Field* field = find(name);
		if (!field || !value) return false;
		while (blank(*value)) value++;
		size_t length = std::strlen(value);
		while (length && blank(value[length - 1])) length--;
		switch (field->type) {
		case Type::text:
			if (length >= field->size) return false;
			std::memcpy(reinterpret_cast<char*>(&settings) + field->offset, value, length);
			reinterpret_cast<char*>(&settings)[field->offset + length] = 0;
			return true;
		case Type::boolean: {
			bool on = equals(value, length, "on") || equals(value, length, "true") || equals(value, length, "1");
			bool off = equals(value, length, "off") || equals(value, length, "false") || equals(value, length, "0");
			if (!on && !off) return false;
			reinterpret_cast<uint8_t*>(&settings)[field->offset] = on ? 1 : 0;
			return true;
		}
		}
		return false;
	}

	size_t format(const Settings& settings, const Field& field, char* out, size_t capacity) {
		if (!capacity) return 0;
		switch (field.type) {
		case Type::text: {
			const char* text = reinterpret_cast<const char*>(&settings) + field.offset;
			size_t length = std::strlen(text);
			if (length >= capacity) length = capacity - 1;
			std::memcpy(out, text, length);
			out[length] = 0;
			return length;
		}
		case Type::boolean: {
			const char* word = reinterpret_cast<const uint8_t*>(&settings)[field.offset] ? "ON" : "OFF";
			size_t length = std::strlen(word);
			if (length >= capacity) length = capacity - 1;
			std::memcpy(out, word, length);
			out[length] = 0;
			return length;
		}
		}
		out[0] = 0;
		return 0;
	}
}
