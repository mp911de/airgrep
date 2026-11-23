#pragma once

#include <atomic>

#include "airgrep.h"
#include "remote/connection.hpp"

struct airgrep_connection {
	explicit airgrep_connection(
		remote::Connection::Open open,
		std::shared_ptr<std::atomic<int64_t>> since_scene_ms = nullptr)
		: fixture_ms(std::move(since_scene_ms)), connection(std::move(open)) {}
	// How long the device of a fixture has run since its scene. None for
	// another device.
	std::shared_ptr<std::atomic<int64_t>> fixture_ms;
	remote::Connection connection;
};
