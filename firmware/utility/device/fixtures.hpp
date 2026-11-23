#pragma once

#include <atomic>
#include <memory>
#include <string_view>

#include "emulator.hpp"

// Devices in a known state, one per scene of docs/ui-design/sketch. A function
// gives the device at the moment of its scene.
namespace device::fixtures {
EmulatedDevice normal();
EmulatedDevice advisory();
EmulatedDevice alert();
EmulatedDevice no_fix();
EmulatedDevice own_ship();
EmulatedDevice busy();
EmulatedDevice beyond();
EmulatedDevice conflict();
EmulatedDevice corrective();
EmulatedDevice preventive();
EmulatedDevice recovery();
EmulatedDevice charging();
EmulatedDevice causes();

// A fixture as a device to talk to. The name is that of the fixture, and
// after it the stored display settings that differ from the sketch: dark,
// tracks, simple-symbols, conflict-indicator, range=NM, language=CODE.
// zone=NAME is the stored time zone of the device, chosen by hand, and
// no-own-ship a device that stores neither address nor callsign. Error for a
// word it does not know.
// The device runs as long past its scene as since_scene_ms says, in whole
// seconds: the own ship and every Target hold their course. Whoever drives the
// device sets it, no clock does. Without it the device stays at its scene.
std::unique_ptr<Transport> open(
	std::string_view name,
	std::shared_ptr<const std::atomic<int64_t>> since_scene_ms = nullptr);
// The GNSS data of the fixture of this name, to hand to another device. Error
// for a name it does not know.
adsbee::Gnss gnss_of(std::string_view name);
}
