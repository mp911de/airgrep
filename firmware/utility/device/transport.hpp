#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace device {
struct Error : std::runtime_error {
	using std::runtime_error::runtime_error;
};

class Transport {
   public:
	Transport() = default;
	Transport(const Transport&) = delete;
	Transport& operator=(const Transport&) = delete;
	Transport(Transport&&) = delete;
	Transport& operator=(Transport&&) = delete;
	virtual ~Transport() = default;
	virtual void reset_session() = 0;
	virtual void write(std::span<const uint8_t> frame) = 0;
	// Appends what arrives within the time, which may be more or less than
	// wanted. Returns as soon as the wanted bytes are there. False when
	// nothing arrived.
	virtual bool read(std::vector<uint8_t>& bytes, size_t wanted,
					  std::chrono::milliseconds timeout) = 0;
};
}
