#include <string.h>
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
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
#include "record.h"
#include "time.h"
#include <stdlib.h>

#define TAG_MAIN  "main"

// 定义队列句柄
static QueueHandle_t gpio_evt_queue = NULL; //gpio isr向录音任务发送消息
static QueueHandle_t debug_queue = NULL;    //其他任务向调试任务发送消息

// 定义信号量句柄
SemaphoreHandle_t Semaphore_is_record_over = NULL;
SemaphoreHandle_t Semaphore_record_over = NULL;

extern int cnt; //全局变量，用于记录录音次数
int random_year = 0;

// gpio中断服务函数
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    //debug消息的代号
    char debug_meg[64];
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // 获取信号量
    if (xSemaphoreTakeFromISR(Semaphore_is_record_over, &xHigherPriorityTaskWoken) == pdTRUE)
    {  
        // 计数，用于记录中断次数，一共10次，超过十次结束
        cnt++;
        if (cnt <= EXAMPLE_MAX_RECORD_TIMES)
        {
            xQueueSendFromISR(gpio_evt_queue, &cnt, NULL);
        }
    }

    else
    {
        stpcpy(debug_meg, "重复按下");
        //释放结束录音的信号量
        xSemaphoreGive(Semaphore_record_over);
    }
  
    if (xHigherPriorityTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }

    //往调试队列发送消息
    xQueueSendFromISR(debug_queue, &debug_meg, NULL);
}

// gpio任务
static void gpio_task(void *arg)
{
    i2s_chan_handle_t i2s_rx_chan_task = (i2s_chan_handle_t)arg;
    uint8_t num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &num, portMAX_DELAY))
        {
            //录音次序
            ESP_LOGI(TAG_MAIN, "第%d次录音", num);
            // 挂载SD卡
            sdmmc_card_t *sdmmc_card = mount_sdcard();

            // 录音
            esp_err_t err = record_wav(i2s_rx_chan_task);

            // 弹出SD卡
            esp_vfs_fat_sdcard_unmount(EXAMPLE_SD_MOUNT_POINT, sdmmc_card);

            if (err == ESP_OK)
            {
                //释放二值信号量
                xSemaphoreGive(Semaphore_is_record_over);
                ESP_LOGI(TAG_MAIN, "成功录音至%s,可安全拔出sd卡", EXAMPLE_RECORD_FILE_PATH);
            }
            else
            {
                ESP_LOGE(TAG_MAIN, "录音失败,sd卡%s处的文件可能无法播放", EXAMPLE_RECORD_FILE_PATH);
            }
        }
        //延时100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 调试任务
static void debug_task(void *arg)
{
    char msg[64];
    for (;;)
    {
        if(xQueueReceive(debug_queue, &msg, portMAX_DELAY))
        {
            if(strcmp(msg, "重复按下") == 0)
            {
                ESP_LOGI(TAG_MAIN, "录音结束");
            }
            else
            {
                ESP_LOGI(TAG_MAIN, "调试信息：%s", msg);
            }
        }
        //延时100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    // 初始化I2Sjieko
    i2s_chan_handle_t i2s_rx_chan = es7210_i2s_init();
    // 初始化ES7210
    es7210_codec_init();

    // 初始化gpio外部中断
    gpio_config_t io0_conf =
        {
            .intr_type = GPIO_INTR_NEGEDGE,  // 下降沿中断
            .mode = GPIO_MODE_INPUT,         // 输入模式
            .pin_bit_mask = 1 << GPIO_NUM_0, // 选择GPIO0
            .pull_down_en = 0,               // 禁能内部下拉
            .pull_up_en = 1                  // 使能内部上拉
        };
    gpio_config(&io0_conf);
    // 创建一个队列
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    debug_queue = xQueueCreate(10, sizeof(char));

    // 创建一个任务,将i2s接收通道句柄作为参数传递给任务
    xTaskCreate(gpio_task, "gpio_task", 4096, (void *)i2s_rx_chan, 10, NULL);

    //创建一个任务用于接受其他任务的消息，负责发送调试信息
    xTaskCreate(debug_task, "debug_task", 2048, NULL, 5, NULL);

    // 创建一个二值信号量，当每次录音完后释放信号量，gpio中断服务函数获取信号量
    Semaphore_is_record_over = xSemaphoreCreateBinary();
    Semaphore_record_over = xSemaphoreCreateBinary();

    // 先释放一次信号量，确保第一次录音可以开始
    xSemaphoreGive(Semaphore_is_record_over);

    // 创建gpio中断服务
    gpio_install_isr_service(0);

    // 添加中断服务函数
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void *)GPIO_NUM_0);

    //生成一个四位数的随机数字作为年份
    /*
    设备没有RTC备用电源，每次启动时时间戳相同，为了规避录音名字重复，随机生成一个四位数字作为当前年份
    只在系统启动时生成一次，这样可以确保单次启动时生成的文件编号接近
    */
    // 初始化随机数生成器
    srand((unsigned) time(NULL));
    // 生成一个四位数的随机数字作为年份
    random_year = rand() % 9000 + 1000;


}
