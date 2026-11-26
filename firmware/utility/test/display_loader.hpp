#pragma once

#include <dlfcn.h>

#include <string>

#include "app/display.h"

namespace display_loader {
inline airgrep_display_getter_ptr load_getter(const char* path,
											  std::string& error) {
	void* library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!library) {
		const char* reason = dlerror();
		error = reason ? reason : "Cannot open display library";
		return nullptr;
	}
	auto* getter = reinterpret_cast<airgrep_display_getter_ptr>(
		dlsym(library, "airgrep_display_get_api"));
	if (!getter) {
		const char* reason = dlerror();
		error = reason ? reason : "Missing airgrep_display_get_api symbol";
		dlclose(library);
		return nullptr;
	}
	return getter;
}

inline const airgrep_display_api* load_api(const char* path,
										   std::string& error) {
	airgrep_display_getter_ptr getter = load_getter(path, error);
	if (!getter) {
		return nullptr;
	}
	const airgrep_display_api* api =
		getter(AIRGREP_DISPLAY_ABI, sizeof(airgrep_display_api));
	if (!api) {
		error = "The display library has another interface";
	}
	return api;
}
}
