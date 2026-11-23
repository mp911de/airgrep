#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../main/usb/usb_constants.h"

// Run airgrep_devices and airgrep_disconnect on a background queue.

#ifdef __cplusplus
extern "C" {
#endif

#define AIRGREP_EMULATED "000000000000"

typedef struct {
	char serial[16];
	bool data;
} airgrep_device;

// Writes the devices on the USB bus, at most capacity, and returns how many.
// The problem is why the bus could not be listed, or empty.
size_t airgrep_devices(airgrep_device* devices, size_t capacity,
					   char problem[256]);

typedef struct airgrep_connection airgrep_connection;

enum { AIRGREP_CONNECTING, AIRGREP_CONNECTED, AIRGREP_ENDED };

typedef struct {
	int phase;
	char error[256];
	bool readable;
	uint64_t uptime_seconds;
	char firmware[128], board[128], chip[128];
	// Health bits, and their names separated by commas.
	uint16_t health;
	char health_names[256];
	char mode[16];
	uint8_t cpu_core0, cpu_core1, cpu_overall;
	uint32_t internal_free, internal_minimum, internal_total;
	uint32_t psram_free, psram_minimum, psram_total;
	uint64_t pictures_read;
	double last_picture_read_ms;
} airgrep_status;

// Starts talking to a device. Never fails here: a device that does not
// answer shows as an ended connection with its reason.
airgrep_connection* airgrep_connect(const char* serial);
// Starts talking to an emulated device in the state of a fixture. No fixture
// of that name shows as an ended connection with its reason.
airgrep_connection* airgrep_connect_fixture(const char* name);
// Lets this much time pass on the device of a fixture: its own ship and its
// Targets fly on. A fixture has no clock of its own.
void airgrep_fixture_pass(airgrep_connection* connection, uint32_t elapsed_ms);
// Ends the conversation and frees the connection: no call may name it again,
// a display that follows it has to stop first. Waits for a request that is on
// its way, up to a few seconds with a silent device.
void airgrep_disconnect(const airgrep_connection* connection);
void airgrep_status_of(const airgrep_connection* connection,
					   airgrep_status* status);
// The device says the time of day and no date. utc_day is the day its time
// belongs to, counted from 1970-01-01. Without this call it is today.
void airgrep_set_day(airgrep_connection* connection, int64_t utc_day);
// A picture every interval for a display to follow, or none for 0.
void airgrep_watch(airgrep_connection* connection, uint32_t interval_ms);
// The newest picture as text. Writes at most capacity bytes with their end
// and returns the length of the whole text, 0 while there is no picture.
size_t airgrep_data_text(const airgrep_connection* connection, char* text,
						 size_t capacity);
// Either restart ends the connection.
void airgrep_reboot(airgrep_connection* connection);
void airgrep_enter_bootloader(airgrep_connection* connection);

void airgrep_reset_settings(airgrep_connection* connection);

#ifdef __cplusplus
}
#endif
