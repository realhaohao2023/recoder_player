#include "record.h"

uint8_t cnt = 0;
extern SemaphoreHandle_t Semaphore_record_over;
extern int random_year;
// es7210初始化
/*
es7210芯片是高性能的音频模数转换器（ADC）
连接3个mic，mic1和2用来接收人说话的声音，mic3接es8311的输出，用于回音消除
通过这个函数初始化es7210芯片i2s接口的接收通道，配置为分时复用模式
*/
i2s_chan_handle_t es7210_i2s_init(void)
{
    // 定义i2s接收通道句柄
    i2s_chan_handle_t i2s_rx_chan = NULL;
    ESP_LOGI(TAG, "创建I2S接收通道");

    // 配置接收通道
    i2s_chan_config_t i2s_rx_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    /*
    参数1：I2S_NUM_AUTO，自动选择I2S号
    参数2：I2S_ROLE_MASTER，主机模式
    */
    // 创建i2s通道
    ESP_ERROR_CHECK(i2s_new_channel(&i2s_rx_conf, NULL, &i2s_rx_chan));
    /*
    i2s_new_channel 参数解释：
     参数1：i2s_rx_conf，i2s配置
     参数2：NULL，i2s配置
     参数3：i2s_rx_chan，储存i2s通道句柄
    */
    ESP_LOGI(TAG, "I2S接收通道配置为TDM模式");
    // tdm：（Time Division Multiplexing）分时复用
    //  定义接收通道为I2S TDM模式 并配置
    i2s_tdm_config_t i2s_tdm_rx_conf =
        {
            .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(EXAMPLE_I2S_SAMPLE_BITS, I2S_SLOT_MODE_STEREO, EXAMPLE_I2S_TDM_SLOT_MASK),
            .clk_cfg =
                {
                    .clk_src = I2S_CLK_SRC_DEFAULT,
                    .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE,
                    .mclk_multiple = EXAMPLE_I2S_MCLK_MULTIPLE},
            // 配置I2S通道引脚
            .gpio_cfg =
                {
                    .mclk = EXAMPLE_I2S_MCK_IO,
                    .bclk = EXAMPLE_I2S_BCK_IO,
                    .ws = EXAMPLE_I2S_WS_IO,
                    .dout = -1,
                    .din = EXAMPLE_I2S_DI_IO}};
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(i2s_rx_chan, &i2s_tdm_rx_conf));
    return i2s_rx_chan;
}

// 挂载sd卡
sdmmc_card_t *mount_sdcard(void)
{
    sdmmc_card_t *sdmmc_card = NULL;

    ESP_LOGI(TAG, "Mounting SD card");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 2,
        .allocation_unit_size = 8 * 1024};

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");

    sdmmc_host_t sdmmc_host = SDMMC_HOST_DEFAULT();                // SDMMC主机接口配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // SDMMC插槽配置
    slot_config.width = 1;                                         // 设置为1线SD模式
    slot_config.clk = EXAMPLE_SD_CLK_IO;
    slot_config.cmd = EXAMPLE_SD_CMD_IO;
    slot_config.d0 = EXAMPLE_SD_DAT0_IO;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 打开内部上拉电阻

    ESP_LOGI(TAG, "Mounting filesystem");

    esp_err_t ret;
    while (1)
    {
        ret = esp_vfs_fat_sdmmc_mount(EXAMPLE_SD_MOUNT_POINT, &sdmmc_host, &slot_config, &mount_config, &sdmmc_card);
        if (ret == ESP_OK)
        {
            break;
        }
        else if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). ", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Card size: %lluMB, speed: %dMHz",
             (((uint64_t)sdmmc_card->csd.capacity) * sdmmc_card->csd.sector_size) >> 20,
             sdmmc_card->max_freq_khz / 1000);

    return sdmmc_card;
}

// 用I2C初始化ES7210
/*
函数功能：配置I2C接口，创建ES7210设备句柄，配置ES7210设备
*/
void es7210_codec_init(void)
{
    // 打印调试接口
    ESP_LOGI(TAG, "初始化I2C接口,用于初始化ES7210设备");
    i2c_config_t i2c_conf =
        {
            .sda_io_num = EXAMPLE_I2C_SDA_IO,
            .scl_io_num = EXAMPLE_I2C_SCL_IO,
            .mode = I2C_MODE_MASTER, // 主机模式
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,       // 使能上拉电阻
            .master.clk_speed = EXAMPLE_ES7210_I2C_CLK // I2C时钟频率
        };
    ESP_ERROR_CHECK(i2c_param_config(EXAMPLE_I2C_NUM, &i2c_conf));                // 配置I2C接口
    ESP_ERROR_CHECK(i2c_driver_install(EXAMPLE_I2C_NUM, i2c_conf.mode, 0, 0, 0)); // 安装I2C驱动

    // 创建ES7210设备句柄
    es7210_dev_handle_t es7210_handle = NULL;
    // 配置I2C接口
    es7210_i2c_config_t es7210_i2c_conf =
        {
            .i2c_port = EXAMPLE_I2C_NUM,
            .i2c_addr = EXAMPLE_ES7210_I2C_ADDR};
    // 通过组件里的库函数创建ES7210设备
    ESP_ERROR_CHECK(es7210_new_codec(&es7210_i2c_conf, &es7210_handle));

    // 配置ES7210设备
    ESP_LOGI(TAG, "配置ES7210设备参数");
    es7210_codec_config_t codec_conf =
        {
            .i2s_format = EXAMPLE_I2S_TDM_FORMAT,
            .mclk_ratio = EXAMPLE_I2S_MCLK_MULTIPLE,
            .sample_rate_hz = EXAMPLE_I2S_SAMPLE_RATE,
            .bit_width = (es7210_i2s_bits_t)EXAMPLE_I2S_SAMPLE_BITS,
            .mic_bias = EXAMPLE_ES7210_MIC_BIAS,
            .mic_gain = EXAMPLE_ES7210_MIC_GAIN,
            .flags.tdm_enable = true};
    ESP_ERROR_CHECK(es7210_config_codec(es7210_handle, &codec_conf));
    ESP_ERROR_CHECK(es7210_config_volume(es7210_handle, EXAMPLE_ES7210_ADC_VOLUME));
}

// 录音
/*
函数功能：录音 用wav格式保存到sd卡
参数：i2s_rx_chan：i2s接收通道句柄
*/
esp_err_t record_wav(i2s_chan_handle_t i2s_rx_chan)
{
    ESP_RETURN_ON_FALSE(i2s_rx_chan, ESP_FAIL, TAG, "I2S接收通道句柄无效");
    esp_err_t ret = ESP_OK;

    // 音频数据的字节率
    uint32_t byte_rate = EXAMPLE_I2S_SAMPLE_RATE * EXAMPLE_I2S_CHAN_NUM * EXAMPLE_I2S_SAMPLE_BITS / 8;
    // wav文件的大小
    uint32_t wav_size = byte_rate * EXAMPLE_RECORD_TIME_SEC;

    // wav文件头的配置
    const wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(wav_size, EXAMPLE_I2S_SAMPLE_BITS, EXAMPLE_I2S_SAMPLE_RATE, EXAMPLE_I2S_CHAN_NUM);

    // 创建或打开wav文件

    // 生成带有编号的文件名

    // 获取当前时间
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 使用随机年份替换实际年份
    timeinfo.tm_year = random_year - 1900;

    // 提取时间戳的各个部分
    int year = timeinfo.tm_year + 1900; // 年份
    int month = timeinfo.tm_mon + 1;    // 月份（tm_mon 从 0 开始计数）
    int day = timeinfo.tm_mday;         // 日期
    int hour = timeinfo.tm_hour;        // 小时
    int minute = timeinfo.tm_min;       // 分钟
    int second = timeinfo.tm_sec;       // 秒数

    // 将各个部分加起来
    int timestamp_sum = year + (month * 100 + day) + (hour * 100 + minute) + second;

    // 打印结果
    ESP_LOGI(TAG, "时间戳各部分之和：%d", timestamp_sum);

    // 创建一个缓冲区
    char file_path[64];
    sprintf(file_path, "%s/REC%d.WAV", EXAMPLE_SD_MOUNT_POINT, timestamp_sum);

    ESP_LOGI(TAG, "打开文件：%s", file_path);
    FILE *f = fopen(file_path, "w");

    ESP_RETURN_ON_FALSE(f, ESP_FAIL, TAG, "打开文件失败");

    // 写入wav文件头
    ESP_GOTO_ON_FALSE(fwrite(&wav_header, sizeof(wav_header_t), 1, f), ESP_FAIL, err, TAG, "error while writing wav header");

    // 开始录音
    size_t wav_written = 0;
    // 定义一个缓冲区
    static int16_t i2s_readraw_buff[4096];
    ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "I2S接收通道使能失败");

    while (wav_written < wav_size)
    {
        if (wav_written % byte_rate < sizeof(i2s_readraw_buff))
        {
            ESP_LOGI(TAG, "Recording: %" PRIu32 "/%ds", wav_written / byte_rate + 1, EXAMPLE_RECORD_TIME_SEC);
        }
        size_t bytes_read = 0;

        // 读取音频数据
        ESP_GOTO_ON_ERROR(
            i2s_channel_read(i2s_rx_chan, i2s_readraw_buff, sizeof(i2s_readraw_buff), &bytes_read, pdMS_TO_TICKS(1000)),
            err, TAG, "I2S接收通道读取数据失败");
        // 写入wav文件
        ESP_GOTO_ON_FALSE(fwrite(i2s_readraw_buff, bytes_read, 1, f), ESP_FAIL, err, TAG, "写入wav文件失败");

        wav_written += bytes_read;

        if (xSemaphoreTake(Semaphore_record_over, 0) == pdTRUE)
        {
            ESP_LOGI(TAG, "录音结束，此次录音时长:%" PRIu32 "s", wav_written / byte_rate);
            break;
        }
    }

err:
    i2s_channel_disable(i2s_rx_chan);
    ESP_LOGI(TAG, "Recording done! Flushing file buffer");
    fclose(f);

    return ret;
}
