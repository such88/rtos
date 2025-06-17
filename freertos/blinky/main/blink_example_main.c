/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GREEN GPIO_NUM_4
#define BLINK_BLUE GPIO_NUM_2

void app_main(void){
    gpio_reset_pin(BLINK_GREEN);
    gpio_set_direction(BLINK_GREEN, GPIO_MODE_OUTPUT);

    while (1) {
        // Blink off (output low)
        gpio_set_level(BLINK_GREEN, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        // Blink on (output high)
        gpio_set_level(BLINK_GREEN, 1);
        vTaskDelay( 100 / portTICK_PERIOD_MS);
    }

}

