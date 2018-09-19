#ifndef NUMBER_DISPLAY_H
#define NUMBER_DISPLAY_H

#include <stdlib.h>
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"

// gpio[0] controls segment A, gio[1] controls segment B...
uint8_t gpio[7] = {
    12,
    13,
    0,
    4,
    5,
    16,
    14
};
#define NUM_DIGIT 2
// select wich digit to set, digitSelect[0] is the least significant digit
uint8_t digitSelect[2] = {
    3
};

// set a pin to 0 to light up the corresponding segment
//         A  B  C  D  E  F  G
// 0 -> 0, 0, 0, 0, 0, 0, 0, 1
uint8_t pinsForNumber[10] = {
    0x01,  // 0, 0, 0, 0, 0, 0, 0, 1    0
    0x4F,  // 0, 1, 0, 0, 1, 1, 1, 1    1
    0x12,  // 0, 0, 0, 1, 0, 0, 1, 0    2
    0x06,  // 0, 0, 0, 0, 0, 1, 1, 0    3
    0x4C,  // 0, 1, 0, 0, 1, 1, 0, 0    4
    0x24,  // 0, 0, 1, 0, 0, 1, 0, 0    5
    0x20,  // 0, 0, 1, 0, 0, 0, 0, 0    6
    0x0F,  // 0, 0, 0, 0, 1, 1, 1, 1    7
    0x00,  // 0, 0, 0, 0, 0, 0, 0, 0    8
    0x04   // 0, 0, 0, 0, 0, 1, 0, 0    9
};

void setDigit(uint16_t number) {
    if(number > 9) {
        number = 8;
    }
    uint8_t i;
    for(i=0; i<7; i++) {
        gpio_write(gpio[i], (pinsForNumber[number]>>(6-i)) & 1);
    }
}


// param: QueueHandle_t *, Queue which holds number which should be displayed
void displayNumber(void *pvArguments) {
    uint16_t number = 0;
    QueueHandle_t *numberQueue = (QueueHandle_t *) pvArguments;
    uint8_t currentDigit;
    while(true) {
        uint16_t powerOfTen = 1;
        // take new number from queue (number isn't changed when no number is in the queue
        xQueueReceive(*numberQueue, &number , 0);
        // set each digit seperatly
        for(currentDigit=0; currentDigit<NUM_DIGIT; currentDigit++) {
            uint8_t j;
            // enable only current digit
            for(j=0; j<=NUM_DIGIT/2; j++) {
                gpio_write(digitSelect[j], currentDigit==j ? 1 : 0);
            }
            setDigit((number/powerOfTen)%10);
            powerOfTen = powerOfTen * 10;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void countNumbers(void *pvArguments) {
    uint8_t i = 0;
    uint16_t *number = (uint16_t *) malloc(sizeof(uint16_t));
    TaskHandle_t displayNumberTask = NULL;
    xTaskCreate(displayNumber, "displayNumberTask", 256, (void *) number, 2, &displayNumberTask);
    while(1) {
        *number = i;
        i=i+1;
        if(i>99) {
            i=0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif /* NUMBER_DISPLAY_H */
