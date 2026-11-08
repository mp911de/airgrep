#include "device_info.h"
#include <string.h>
#include "board.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "sdkconfig.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif

const char* device_reset_reason(void) {
	switch (esp_reset_reason()) {
	case ESP_RST_POWERON: return "POWER ON";
	case ESP_RST_EXT: return "EXTERNAL PIN";
	case ESP_RST_SW: return "SOFTWARE";
	case ESP_RST_PANIC: return "PANIC";
	case ESP_RST_INT_WDT: return "INTERRUPT WATCHDOG";
	case ESP_RST_TASK_WDT: return "TASK WATCHDOG";
	case ESP_RST_WDT: return "WATCHDOG";
	case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
	case ESP_RST_BROWNOUT: return "BROWNOUT";
	case ESP_RST_SDIO: return "SDIO";
	case ESP_RST_USB: return "USB";
	case ESP_RST_JTAG: return "JTAG";
	case ESP_RST_EFUSE: return "EFUSE ERROR";
	case ESP_RST_PWR_GLITCH: return "POWER GLITCH";
	case ESP_RST_CPU_LOCKUP: return "CPU LOCKUP";
	default: return "UNKNOWN";
	}
}

void device_id_format(const uint8_t mac[6], char out[21]) {
	snprintf(out, 21, "airgrep-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static uint8_t efuse_bits(const esp_efuse_desc_t* field[], size_t bits) {
	uint8_t value = 0;
	esp_efuse_read_field_blob(field, &value, bits);
	return value;
}

const device_info_t* device_info(void) {
	static device_info_t info;
	static bool read;
	if (read) return &info;
	esp_chip_info_t chip;
	esp_chip_info(&chip);
	info.revision = chip.revision;
	info.cores = chip.cores;
	info.package = efuse_bits(ESP_EFUSE_PKG_VERSION, 3);
	info.flash_cap = efuse_bits(ESP_EFUSE_FLASH_CAP, 3);
	info.psram_cap = efuse_bits(ESP_EFUSE_PSRAM_CAP, 3);
	info.psram_vendor = efuse_bits(ESP_EFUSE_PSRAM_VENDOR, 2);
	esp_flash_read_id(NULL, &info.flash_jedec_id);
	esp_flash_get_physical_size(NULL, &info.flash_bytes);
#if CONFIG_SPIRAM
	if (esp_psram_is_initialized()) info.psram_bytes = esp_psram_get_size();
#endif
	esp_read_mac(info.mac, ESP_MAC_EFUSE_FACTORY);
	device_id_format(info.mac, info.device_id);
	read = true;
	return &info;
}

void device_info_print(int width) {
	const device_info_t* info = device_info();
	const esp_app_desc_t* app = esp_app_get_description();
	// esptool's ESP32-S3 package names by PKG_VERSION.
	static const char* packages[] = {"QFN56", "PICO-1 LGA56"};
	// Efuse FLASH_CAP, PSRAM_CAP and PSRAM_VENDOR value tables from esp_efuse_table.csv.
	static const char* flash_caps[] = {"none", "8 MB", "4 MB"};
	static const char* psram_caps[] = {"none", "8 MB", "2 MB", "16 MB", "4 MB"};
	static const char* psram_vendors[] = {"", "AP 3.3 V", "AP 1.8 V"};
	const char* package = info->package < 2 ? packages[info->package] : "unknown package";
	const char* flash_cap = info->flash_cap < 3 ? flash_caps[info->flash_cap] : "?";
	const char* psram_cap = info->psram_cap < 5 ? psram_caps[info->psram_cap] : "?";
	const char* psram_vendor = info->psram_vendor < 3 ? psram_vendors[info->psram_vendor] : "?";
	printf("%-*s %s %s (%s %s)\n", width, "Project", app->project_name, app->version, app->date, app->time);
	printf("%-*s %s\n", width, "IDF", app->idf_ver);
	printf("%-*s %s\n", width, "Board", BOARD_NAME);
	printf("%-*s ESP32-S3 (%s) rev v%u.%u, %u cores @ %u MHz\n", width, "Chip", package,
			info->revision / 100, info->revision % 100, info->cores, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
	printf("%-*s Flash %lu MB (JEDEC 0x%06lX, embedded %s), PSRAM %u MB (embedded %s %s)\n", width, "Memory",
			(unsigned long)(info->flash_bytes >> 20), (unsigned long)info->flash_jedec_id, flash_cap,
			(unsigned)(info->psram_bytes >> 20), psram_cap, psram_vendor);
	printf("%-*s %02X:%02X:%02X:%02X:%02X:%02X\n", width, "MAC", info->mac[0], info->mac[1], info->mac[2],
			info->mac[3], info->mac[4], info->mac[5]);
	printf("%-*s %s\n", width, "Device ID", info->device_id);
	printf("%-*s %s\n", width, "Last reset", device_reset_reason());
}
