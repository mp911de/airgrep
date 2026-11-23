#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565

#define LV_USE_OBJ_NAME 1
#define LV_USE_TRANSLATION 1
#define LV_USE_QRCODE 1

/* The 800x480 user interface takes 300 KB at Home and 400 KB with 32 targets
   and their tracks. */
#define LV_MEM_SIZE (1024 * 1024U)

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#endif
