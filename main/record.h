#pragma once

#include <string.h>
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "driver/i2s_tdm.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "es7210.h"
#include "format_wav.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "time.h"
#include <stdlib.h>


// 引脚参数宏定义
/* I2C port and GPIOs */
#define EXAMPLE_I2C_NUM (0)
#define EXAMPLE_I2C_SDA_IO (1)
#define EXAMPLE_I2C_SCL_IO (2)

/* I2S port and GPIOs */
#define EXAMPLE_I2S_NUM (0)
#define EXAMPLE_I2S_MCK_IO (38)
#define EXAMPLE_I2S_BCK_IO (14)
#define EXAMPLE_I2S_WS_IO (13)
#define EXAMPLE_I2S_DI_IO (12)

/* SD card GPIOs */
#define EXAMPLE_SD_CMD_IO (48)
#define EXAMPLE_SD_CLK_IO (47)
#define EXAMPLE_SD_DAT0_IO (21)

// I2S参数宏定义
/* I2S configurations */
#define EXAMPLE_I2S_TDM_FORMAT (ES7210_I2S_FMT_I2S)
#define EXAMPLE_I2S_CHAN_NUM (2)
#define EXAMPLE_I2S_SAMPLE_RATE (48000)
#define EXAMPLE_I2S_MCLK_MULTIPLE (I2S_MCLK_MULTIPLE_256)
#define EXAMPLE_I2S_SAMPLE_BITS (I2S_DATA_BIT_WIDTH_16BIT)
#define EXAMPLE_I2S_TDM_SLOT_MASK (I2S_TDM_SLOT0 | I2S_TDM_SLOT1)

// ES7210参数宏定义
#define EXAMPLE_ES7210_I2C_ADDR (0x41)
#define EXAMPLE_ES7210_I2C_CLK (100000)
#define EXAMPLE_ES7210_MIC_GAIN (ES7210_MIC_GAIN_30DB)
#define EXAMPLE_ES7210_MIC_BIAS (ES7210_MIC_BIAS_2V87)
#define EXAMPLE_ES7210_ADC_VOLUME (0)

// 录音参数宏定义
#define EXAMPLE_RECORD_TIME_SEC (60)
#define EXAMPLE_SD_MOUNT_POINT "/sdcard"
#define EXAMPLE_RECORD_FILE_PATH "/RECORD.WAV"

// 最大录音次数宏定义
#define EXAMPLE_MAX_RECORD_TIMES (10)

#define TAG "record"


i2s_chan_handle_t es7210_i2s_init(void);
sdmmc_card_t *mount_sdcard(void);
void es7210_codec_init(void);
esp_err_t record_wav(i2s_chan_handle_t i2s_rx_chan);