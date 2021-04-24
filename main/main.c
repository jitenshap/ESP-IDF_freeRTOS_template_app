/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "string.h"
#include "driver/gpio.h"
QueueHandle_t main_task_queue;
EventGroupHandle_t main_task_group;
const int TASK_DONE_BIT = BIT0;

//テスト用タスク引数構造体
typedef struct task_arguments_t
{
    int delay_duration;
    int loop_num;
} task_arguments_t;

//Lチカパラメーター
typedef struct led_parameters_t
{
    int led_pin;
    int blink_interval;
} led_parameters_t;

//タスク引数受け取るテスト
void sub_slow_task_1(void * pvParameters)
{
    task_arguments_t * task_args = (task_arguments_t *)pvParameters;
    const char * TAG = "SUB_TASK_1";
    for(int i = 0; i < 5; i ++)
    {
        ESP_LOGI(TAG, "Slow task running [%d/%d]", (i + 1), (task_args -> loop_num));
        vTaskDelay((task_args -> delay_duration) / portTICK_RATE_MS);
    }
    xEventGroupSetBits(main_task_group, TASK_DONE_BIT);
    vTaskDelete(NULL);
}

//queueで所要時間を返して終了させるテスト
void sub_slow_task_2(void * pvParameters)
{
    const char * TAG = "SUB_TASK_2";
    uint64_t send_data = esp_timer_get_time();
    for(int i = 0; i < 10; i ++)
    {
        ESP_LOGI(TAG, "Slow task running [%d/10]", (i + 1));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    send_data = esp_timer_get_time() - send_data;
    xQueueSend(main_task_queue, &send_data, portMAX_DELAY);
    vTaskDelete(NULL);
}

//Lチカ
void blink_task(void * pvParameters)
{
    const char * TAG = "LED";
    led_parameters_t * led_params = (led_parameters_t *)pvParameters;
    gpio_reset_pin(led_params -> led_pin);
    gpio_set_direction(led_params -> led_pin, GPIO_MODE_OUTPUT);   
    bool led_enabled = false;
    while(true)
    {
        if(led_enabled)
            gpio_set_level(led_params -> led_pin, 1);
        else
            gpio_set_level(led_params -> led_pin, 0);
        led_enabled = !led_enabled;
        vTaskDelay(led_params -> blink_interval / portTICK_PERIOD_MS);
    }
}

void main_task(void * pvParameters)
{
    const char * TAG = "MAIN_TASK";
    main_task_group = xEventGroupCreate();
    main_task_queue = xQueueCreate(1, sizeof(uint64_t));
    uint64_t received_data;
    while(true)
    {
        EventBits_t uxBits;
        task_arguments_t task_args = 
        {
            .delay_duration = 1000,
            .loop_num = 5
        };
        //順次タスク実行
        xTaskCreate(sub_slow_task_1, "s_task_1", 4096, &task_args, 2, NULL);
        uxBits = xEventGroupWaitBits(main_task_group, TASK_DONE_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        xTaskCreate(sub_slow_task_2, "s_task_2", 4096, NULL, 2, NULL);
        xQueueReceive(main_task_queue, &received_data, portMAX_DELAY);
        ESP_LOGI(TAG, "Received: %llu", received_data);
    }
}

void app_main(void)
{
    esp_timer_init();
    led_parameters_t led_params =
    {
        .led_pin = 10, 
        .blink_interval = 1000
    };
    xTaskCreate(blink_task, "blink_task", 4096, &led_params, 2, NULL);
    xTaskCreate(main_task, "main_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
    while(true)
    {
        size_t free_heap = xPortGetFreeHeapSize();
        ESP_LOGI("MONITOR", "Free heap = %d", free_heap);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}