#include <stdlib.h>
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "esp8266.h"
#include "networking.h"
#include "number_display.h"

static QueueHandle_t displayNumberQueue;

void user_init(void){
    uart_set_baud(0, 115200);
    vTaskDelay(pdMS_TO_TICKS(5000));
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    /*numberToDisplay = (uint16_t *) malloc(sizeof(uint16_t));
    if(numberToDisplay == NULL) {
        return;
        }*/
    displayNumberQueue = xQueueCreate(1, sizeof(uint16_t));
    if(displayNumberQueue == NULL) {
        return;
    }
    //xTaskCreate(wifi_task, "wifi_task", 256, NULL, 4, NULL);
    //xTaskCreate(countNumbers, "countingTask", 256, NULL, 2, NULL);
    xTaskCreate(displayNumber, "displayNumberTask", 256, &displayNumberQueue, 2, NULL);
    xTaskCreate(time_update_task, "time_update_task", 1024, &displayNumberQueue, 3, NULL);
    vSemaphoreCreateBinary(wifi_alive);
    uint8_t i;
    for(i=0; i<7; i++) {
        gpio_enable(gpio[i], GPIO_OUTPUT);
    }
    for(i=0; i<2; i++) {
        gpio_enable(digitSelect[i], GPIO_OUTPUT);
    }
}
