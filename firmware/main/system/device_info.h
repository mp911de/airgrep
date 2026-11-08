#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Facts about this chip and board that identify a device in a bug report.
// Read once, the efuse and flash id never change at run time.
typedef struct {
	uint16_t revision; // MXX, wafer major and minor
	uint8_t cores;
	uint8_t package; // efuse PKG_VERSION
	uint8_t flash_cap, psram_cap, psram_vendor; // efuse in-package memory fields
	uint32_t flash_jedec_id;
	uint32_t flash_bytes; // external flash as detected
	size_t psram_bytes; // 0 when PSRAM is off or absent
	uint8_t mac[6]; // factory MAC
	char device_id[8 + 12 + 1]; // airgrep-<mac>
} device_info_t;

const device_info_t* device_info(void);
const char* device_reset_reason(void);
// Formats airgrep-<mac lower case hex>. Exposed for the host test.
void device_id_format(const uint8_t mac[6], char out[21]);
// Prints the identity rows: Project, IDF, Board, Chip, Memory, MAC, Device ID, Last reset.
void device_info_print(int label_width);

#ifdef __cplusplus
}
#endif
