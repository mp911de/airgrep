#include "usb_console.hpp"

#include "sdkconfig.h"

#if CONFIG_TINYUSB_CDC_ENABLED
#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include "esp_timer.h"
#include "esp_vfs.h"
#include "esp_vfs_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tinyusb.h"

namespace {
constexpr uint8_t kConsole = 0;
constexpr int64_t kWriteTimeoutUs = 3000000;
constexpr char kPath[] = "/dev/usbcon";
static_assert(sizeof(kPath) - 1 <= ESP_VFS_PATH_MAX);
SemaphoreHandle_t read_mutex, write_mutex;
std::atomic<int> modes[3] = {-1, -1, -1};
bool previous_cr;
bool initialized;
std::atomic<bool> active;

std::atomic<TaskHandle_t> service_task;
bool must_use_nonblocking_io() {
	auto owner = service_task.load();
	return !owner || xTaskGetCurrentTaskHandle() == owner;
}

ssize_t dropped_output(size_t size) {
	if (!std::strcmp(pcTaskGetName(nullptr), "console_repl")) {
		// A lost cursor query must fail before linenoise waits for its reply.
		// The IDF REPL retries after 10 ms without holding the stdio lock.
		errno = EAGAIN;
		return -1;
	}
	return static_cast<ssize_t>(size);
}

int open_console(void*, const char*, int flags, int) {
	for (int fd = 0; fd < 3; ++fd) {
		int unused = -1;
		if (modes[fd].compare_exchange_strong(unused, flags)) {
			return fd;
		}
	}
	errno = EMFILE;
	return -1;
}

int close_console(void*, int fd) {
	modes[fd] = -1;
	return 0;
}

int stat_console(void*, int, struct stat* result) {
	*result = {};
	result->st_mode = S_IFCHR;
	return 0;
}

int control_console(void*, int fd, int command, int value) {
	if (command == F_GETFL) {
		return modes[fd];
	}
	if (command == F_SETFL) {
		modes[fd] = value;
		return 0;
	}
	errno = ENOSYS;
	return -1;
}

ssize_t read_console(void*, int fd, void* output, size_t size) {
	if (!size || !active) {
		return 0;
	}
	const bool nonblocking =
		(modes[fd] & O_NONBLOCK) || must_use_nonblocking_io();
	if (xSemaphoreTake(read_mutex, nonblocking ? 0 : portMAX_DELAY) != pdTRUE) {
		errno = EWOULDBLOCK;
		return -1;
	}
	size_t received = 0;
	while (active) {
		received = tud_cdc_n_read(kConsole, output, size);
		if (received || nonblocking) {
			break;
		}
		vTaskDelay(1);
	}
	xSemaphoreGive(read_mutex);
	if (!active) {
		return 0;
	}
	if (!received) {
		errno = EWOULDBLOCK;
		return -1;
	}
	// IDF's console expects LF for Enter, while serial terminals send CR.
	auto* bytes = static_cast<char*>(output);
	std::replace(bytes, bytes + received, '\r', '\n');
	// A disconnected REPL write leaves an error on its stdio stream.
	// Newlib discards the tail of each printf until that error is cleared.
	if (std::memchr(bytes, '\n', received)) {
		std::clearerr(stdout);
		std::clearerr(stderr);
	}
	return static_cast<ssize_t>(received);
}

ssize_t write_console(void*, int, const void* data, size_t size) {
	if (!size || !active) {
		return static_cast<ssize_t>(size);
	}
	const bool nonblocking = must_use_nonblocking_io();
	const int64_t deadline = esp_timer_get_time() + kWriteTimeoutUs;
	if (xSemaphoreTake(write_mutex, nonblocking ? 0 : pdMS_TO_TICKS(3000)) !=
		pdTRUE) {
		return dropped_output(size);
	}
	if (!active || !tud_cdc_n_connected(kConsole)) {
		xSemaphoreGive(write_mutex);
		return dropped_output(size);
	}
	const auto* bytes = static_cast<const char*>(data);
	for (size_t i = 0; i < size && active; ++i) {
		char encoded[2] = {bytes[i], 0};
		size_t count = 1;
		if (bytes[i] == '\n' && !previous_cr) {
			encoded[0] = '\r';
			encoded[1] = '\n';
			count = 2;
		}
		while (active && tud_cdc_n_write_available(kConsole) < count) {
			tud_cdc_n_write_flush(kConsole);
			if (nonblocking || !tud_cdc_n_connected(kConsole) ||
				esp_timer_get_time() >= deadline) {
				xSemaphoreGive(write_mutex);
				return dropped_output(size);
			}
			vTaskDelay(1);
		}
		if (!active) {
			break;
		}
		tud_cdc_n_write(kConsole, encoded, count);
		previous_cr = bytes[i] == '\r';
	}
	if (active) {
		tud_cdc_n_write_flush(kConsole);
	}
	xSemaphoreGive(write_mutex);
	return static_cast<ssize_t>(size);
}

int flush_console(void*, int) {
	if (active &&
		xSemaphoreTake(write_mutex,
					   must_use_nonblocking_io() ? 0 : pdMS_TO_TICKS(3000)) ==
			pdTRUE) {
		if (active) {
			tud_cdc_n_write_flush(kConsole);
		}
		xSemaphoreGive(write_mutex);
	}
	return 0;
}
}

void usb_console_set_service_task() {
	service_task = xTaskGetCurrentTaskHandle();
}

esp_err_t usb_console_init() {
	read_mutex = xSemaphoreCreateMutex();
	write_mutex = xSemaphoreCreateMutex();
	if (!read_mutex || !write_mutex) {
		return ESP_ERR_NO_MEM;
	}
	static const esp_vfs_fs_ops_t operations = {
		.write_p = write_console,
		.lseek_p = nullptr,
		.read_p = read_console,
		.pread_p = nullptr,
		.pwrite_p = nullptr,
		.open_p = open_console,
		.close_p = close_console,
		.fstat_p = stat_console,
		.fcntl_p = control_console,
		.ioctl_p = nullptr,
		.fsync_p = flush_console,
#if CONFIG_VFS_SUPPORT_DIR
		.dir = nullptr,
#endif
#if CONFIG_VFS_SUPPORT_TERMIOS
		.termios = nullptr,
#endif
#if CONFIG_VFS_SUPPORT_SELECT
		.select = nullptr,
#endif
	};
	const esp_err_t result = esp_vfs_register_fs(
		kPath, &operations, ESP_VFS_FLAG_STATIC | ESP_VFS_FLAG_CONTEXT_PTR,
		nullptr);
	if (result != ESP_OK) {
		return result;
	}
	// NOLINTBEGIN(cppcoreguidelines-owning-memory): freopen returns the stream
	// it was given.
	if (!std::freopen(kPath, "r", stdin) || !std::freopen(kPath, "w", stdout) ||
		!std::freopen(kPath, "w", stderr)) {
		return ESP_FAIL;
	}
	// NOLINTEND(cppcoreguidelines-owning-memory)
	(void)setvbuf(stdin, nullptr, _IONBF, 0);
	(void)setvbuf(stdout, nullptr, _IONBF, 0);
	(void)setvbuf(stderr, nullptr, _IONBF, 0);
	initialized = true;
	active = true;
	return ESP_OK;
}

void usb_console_stop() {
	active = false;
	if (initialized) {
		// Drain readers and writers before TinyUSB deletes its FIFO mutexes.
		xSemaphoreTake(read_mutex, portMAX_DELAY);
		xSemaphoreTake(write_mutex, portMAX_DELAY);
		xSemaphoreGive(write_mutex);
		xSemaphoreGive(read_mutex);
	}
}

extern "C" esp_err_t __wrap_esp_stdio_install_io_driver() {
	// IDF 6.1's generic REPL otherwise rejects CONFIG_ESP_CONSOLE_NONE.
	return active ? ESP_OK : ESP_ERR_INVALID_STATE;
}
#else
void usb_console_set_service_task() {}
esp_err_t usb_console_init() { return ESP_OK; }
void usb_console_stop() {}
#endif
