#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "state_error_code.h"

static const char *TAG = "ETX_PUSH_BUTTON";

#define BLINK_GREEN 4
#define BLINK_BLUE 2
#define BLINK_RED 27
#define BTN_GPIO 25

/* press button:
    1: Button press turn on GREEN LED, release turn off GREEN LED
    2: Button press count reach multiple of 4 then GREEN LED toggle count times and OFF and BLUE LED ON
    3: Connect LED RED to GPIO 27, it is always ON
    4: Button press count reach 20 then reset the count to 0
*/
#define CHECK_MSEC  5   // Read hardware every 5 msec
#define PRESS_MSEC  10  // Stable time before registering pressed
#define RELEASE_MSEC    100 // Stable time before registering released

// This holds the debounced state of the key.
bool DebouncedKeyPress = false;

bool RawKeyPressed() {
    return (gpio_get_level(BTN_GPIO) == 0);
}

uint32_t digitalRead() {
    return gpio_get_level(BTN_GPIO);
}

void DebounceSwitch(bool *Key_changed, bool *Key_pressed) {
    static uint8_t Count = RELEASE_MSEC / CHECK_MSEC;
    volatile bool RawState;
    *Key_changed = false;
    *Key_pressed = DebouncedKeyPress;
    RawState = RawKeyPressed();
    if (RawState == DebouncedKeyPress) {
        // Set the timer which will allow a change from the current state.
        if (DebouncedKeyPress) Count = RELEASE_MSEC / CHECK_MSEC;
        else                 Count = PRESS_MSEC / CHECK_MSEC;
    } else {
        printf("KEY Changed\n");
        // Key has changed - wait for new state to become stable.
        if (--Count == 0) {
            // Timer expired - accept the change.
            DebouncedKeyPress = RawState;
            *Key_changed=true;
            *Key_pressed=DebouncedKeyPress;
            // And reset the timer.
            if (DebouncedKeyPress) Count = RELEASE_MSEC / CHECK_MSEC;
            else Count = PRESS_MSEC / CHECK_MSEC;
        }
    }


    // static uint16_t state = 0;
    // state = (state << 1) | digitalRead() | 0xFE00;
    // return (state == 0xFF00);
}

void app_main(void)
{
    /* Reset the pin */
    static uint8_t count = 0;
    volatile bool Key_changed = OFF;
    volatile bool Key_pressed = OFF;

    gpio_reset_pin(BLINK_GREEN);
    gpio_reset_pin(BLINK_BLUE);
    gpio_reset_pin(BTN_GPIO);
    gpio_reset_pin(BLINK_RED);

    /* Set the GPIOs to Output mode */
    gpio_set_direction(BLINK_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK_BLUE, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BTN_GPIO, GPIO_MODE_INPUT);

    /* Enable Pullup for Input Pin */
    gpio_pullup_en(BTN_GPIO);
    gpio_pulldown_dis(BTN_GPIO);
    gpio_set_level(BLINK_RED, ON);

    printf("BLINK_RED gpio_get_level: %u\n", gpio_get_level(BLINK_RED));

    while (1) 
    {
        //btn_prev_state = gpio_get_level(BTN_GPIO);
        DebounceSwitch(&Key_changed, &Key_pressed);
        #if 1
        if(Key_changed == true && Key_pressed == true){
            printf("KEY Pressed\n");
            /* Button is pressed. Turn on the GREEN */
            gpio_set_level(BLINK_GREEN, ON);
            gpio_set_level(BLINK_BLUE, OFF);

            count++;
            if(0 == (count%4)){
                uint32_t l_count = count;
                printf("%lu \n", l_count);
                while(l_count){
                    gpio_set_level(BLINK_GREEN, OFF);
                    vTaskDelay( 100 / portTICK_PERIOD_MS);
                    gpio_set_level(BLINK_GREEN, ON);
                    vTaskDelay( 100 / portTICK_PERIOD_MS);
                    l_count--;
                }
                gpio_set_level(BLINK_GREEN, OFF);
                /* Turn on the BLUE LED */
                gpio_set_level(BLINK_BLUE, ON);
            }
        }
        /* Key release state */
        else if(Key_changed == true && Key_pressed == false) {
            printf("KEY Released\n");
            /* Button is released. Turn off the LED */
            gpio_set_level(BLINK_GREEN, OFF);
            //gpio_set_level(BLINK_BLUE, OFF);

        }
        else{ /* Else key hasn't changed */
           // printf("-------------\n");
            /* start blue LED Toggle */
           //vTaskDelay( 2000 / portTICK_PERIOD_MS);
           // gpio_set_level(BLINK_BLUE, ON);
           // vTaskDelay( 1000 / portTICK_PERIOD_MS);
           // gpio_set_level(BLINK_BLUE, OFF);
        }
        #endif

        if(count ==20){
            count =0;
        }
    }

}

