#include "device_info.h"

#include <string.h>

#include "board.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "sdkconfig.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif

// ESP-IDF places this immediately after esp_app_desc_t in the first image
// segment.
static const char board_description[]
	__attribute__((section(".rodata_custom_desc"), aligned(1))) =
		"airgrep-board:" BOARD_NAME;

const char* device_board_name(void) {
	return board_description + sizeof("airgrep-board:") - 1;
}

const char* device_variant(void) { return AIRGREP_VARIANT; }

const char* device_reset_reason(void) {
	switch (esp_reset_reason()) {
		case ESP_RST_POWERON:
			return "POWER ON";
		case ESP_RST_EXT:
			return "EXTERNAL PIN";
		case ESP_RST_SW:
			return "SOFTWARE";
		case ESP_RST_PANIC:
			return "PANIC";
		case ESP_RST_INT_WDT:
			return "INTERRUPT WATCHDOG";
		case ESP_RST_TASK_WDT:
			return "TASK WATCHDOG";
		case ESP_RST_WDT:
			return "WATCHDOG";
		case ESP_RST_DEEPSLEEP:
			return "DEEP SLEEP";
		case ESP_RST_BROWNOUT:
			return "BROWNOUT";
		case ESP_RST_SDIO:
			return "SDIO";
		case ESP_RST_USB:
			return "USB";
		case ESP_RST_JTAG:
			return "JTAG";
		case ESP_RST_EFUSE:
			return "EFUSE ERROR";
		case ESP_RST_PWR_GLITCH:
			return "POWER GLITCH";
		case ESP_RST_CPU_LOCKUP:
			return "CPU LOCKUP";
		default:
			return "UNKNOWN";
	}
}

void device_id_format(const uint8_t mac[6], char out[21]) {
	(void)snprintf(out, 21, "airgrep-%02x%02x%02x%02x%02x%02x", mac[0], mac[1],
				   mac[2], mac[3], mac[4], mac[5]);
}

const char* device_serial(void) {
	return device_info()->device_id + sizeof("airgrep-") - 1;
}

// The names of an eFuse field's values. A target without the field reads 0
// and names it unknown.
typedef struct {
	const char* const* names;
	size_t count;
} efuse_names_t;

#define EFUSE_NAMES(names) {(names), sizeof(names) / sizeof((names)[0])}

#if CONFIG_IDF_TARGET_ESP32S3
#define CHIP_NAME "ESP32-S3"
static uint8_t efuse_bits(const esp_efuse_desc_t* field[], size_t bits) {
	uint8_t value = 0;
	esp_efuse_read_field_blob(field, &value, bits);
	return value;
}

// esptool's package names by PKG_VERSION, the in-package memory by the value
// tables of esp_efuse_table.csv.
static const char* const packages[] = {"QFN56", "PICO-1 LGA56"};
static const char* const flash_caps[] = {"none", "8 MB", "4 MB"};
static const char* const psram_caps[] = {"none", "8 MB", "2 MB", "16 MB",
										 "4 MB"};
static const char* const psram_vendors[] = {"", "AP 3.3 V", "AP 1.8 V"};
static const efuse_names_t package_names = EFUSE_NAMES(packages);
static const efuse_names_t flash_cap_names = EFUSE_NAMES(flash_caps);
static const efuse_names_t psram_cap_names = EFUSE_NAMES(psram_caps);
static const efuse_names_t psram_vendor_names = EFUSE_NAMES(psram_vendors);

static void efuse_read_package(device_info_t* info) {
	info->package = efuse_bits(ESP_EFUSE_PKG_VERSION, 3);
	info->flash_cap = efuse_bits(ESP_EFUSE_FLASH_CAP, 3);
	info->psram_cap = efuse_bits(ESP_EFUSE_PSRAM_CAP, 3);
	info->psram_vendor = efuse_bits(ESP_EFUSE_PSRAM_VENDOR, 2);
}
#else
#define CHIP_NAME CONFIG_IDF_TARGET
static const efuse_names_t package_names = {0};
static const efuse_names_t flash_cap_names = {0};
static const efuse_names_t psram_cap_names = {0};
static const efuse_names_t psram_vendor_names = {0};

static void efuse_read_package(device_info_t* info) { (void)info; }
#endif

static const char* efuse_name(const efuse_names_t* names, uint8_t value,
							  const char* unknown) {
	return value < names->count ? names->names[value] : unknown;
}

const device_info_t* device_info(void) {
	static device_info_t info;
	static bool read;
	if (read) {
		return &info;
	}
	esp_chip_info_t chip;
	esp_chip_info(&chip);
	info.revision = chip.revision;
	info.cores = chip.cores;
	efuse_read_package(&info);
	esp_flash_read_id(NULL, &info.flash_jedec_id);
	esp_flash_get_physical_size(NULL, &info.detected_flash_bytes);
#if CONFIG_SPIRAM
	if (esp_psram_is_initialized()) {
		info.psram_bytes = esp_psram_get_size();
	}
#endif
	esp_read_mac(info.factory_mac, ESP_MAC_EFUSE_FACTORY);
	device_id_format(info.factory_mac, info.device_id);
	read = true;
	return &info;
}

void device_firmware_format(char* out, size_t size) {
	const esp_app_desc_t* app = esp_app_get_description();
	(void)snprintf(out, size, "%s %s (%s %s)", app->project_name, app->version,
				   app->date, app->time);
}

void device_chip_format(char* out, size_t size) {
	const device_info_t* info = device_info();
	const char* package =
		efuse_name(&package_names, info->package, "unknown package");
	esp_pm_config_t clock = {
		.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
		.min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
	};
	esp_pm_get_configuration(&clock);
	char scaling[16] = "";
	if (clock.min_freq_mhz != clock.max_freq_mhz) {
		(void)snprintf(scaling, sizeof scaling, "%d to ", clock.min_freq_mhz);
	}
	(void)snprintf(out, size, CHIP_NAME " (%s) rev v%u.%u, %u cores @ %s%d MHz",
				   package, info->revision / 100, info->revision % 100,
				   info->cores, scaling, clock.max_freq_mhz);
}

void device_info_print(int width) {
	const device_info_t* info = device_info();
	const esp_app_desc_t* app = esp_app_get_description();
	const char* flash_cap = efuse_name(&flash_cap_names, info->flash_cap, "?");
	const char* psram_cap = efuse_name(&psram_cap_names, info->psram_cap, "?");
	const char* psram_vendor =
		efuse_name(&psram_vendor_names, info->psram_vendor, "?");
	char row[128];
	device_firmware_format(row, sizeof row);
	printf("%-*s %s\n", width, "Firmware", row);
	printf("%-*s %s\n", width, "Variant", device_variant());
	printf("%-*s %s\n", width, "IDF", app->idf_ver);
	printf("%-*s %s\n", width, "Board", device_board_name());
	device_chip_format(row, sizeof row);
	printf("%-*s %s\n", width, "Chip", row);
	printf(
		"%-*s Flash %lu MB (JEDEC 0x%06lX, embedded %s), PSRAM %u MB "
		"(embedded %s %s)\n",
		width, "Memory", (unsigned long)(info->detected_flash_bytes >> 20),
		(unsigned long)info->flash_jedec_id, flash_cap,
		(unsigned)(info->psram_bytes >> 20), psram_cap, psram_vendor);
	printf("%-*s %02X:%02X:%02X:%02X:%02X:%02X\n", width, "MAC",
		   info->factory_mac[0], info->factory_mac[1], info->factory_mac[2],
		   info->factory_mac[3], info->factory_mac[4], info->factory_mac[5]);
	printf("%-*s %s\n", width, "Device ID", info->device_id);
	printf("%-*s %s\n", width, "Last reset", device_reset_reason());
}
