#ifndef WATCHDOG_NETWORKING_H
#define WATCHDOG_NETWORKING_H

#include <stdlib.h>
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <semphr.h>
#include "queue.h"
#include "esp8266.h"
#include <string.h>
#include <lwip/api.h>
#include "number_display.h"
#include <ssid_config.h>

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>

//#define PRINT_DEBUG
#ifdef PRINT_DEBUG
#define printd(s, ...) printf(s, ##__VA_ARGS__)
#else
#define printd(s, ...) do {} while(0) // to avoid lonely semicolons
#endif

#define LOCAL_PORT 4242 // ip of this esp8266
#define SERVER_IP "192.168.1.126" // commas are necessary
#define SERVER_PORT 6900

SemaphoreHandle_t wifi_alive;

// param: QueueHandle_t *, queue to write received numbers to
void time_update(QueueHandle_t *numberQueue) {
    // initialize udp connection
    struct netconn *serverConn = netconn_new(NETCONN_UDP);
    if(netconn_bind(serverConn, IP_ADDR_ANY, LOCAL_PORT) != ERR_OK) {
        printd("couldn't bind address\n");
        return;
    }
    ip_addr_t serverAddr;
    ip4addr_aton(SERVER_IP, &serverAddr);
    //IP4_ADDR(&serverAddr, SERVER_IP);
    if(netconn_connect(serverConn, &serverAddr, SERVER_PORT) != ERR_OK) {
        printd("couldn't connect\n");
    }
    struct netbuf *netbuf = netbuf_new();
    if(netbuf == NULL) {
        printd("couldn't allocate netbuffer\n");
        return;
    }

    netconn_set_recvtimeout(serverConn, 5000);

    // start to send and receive messages
    void *buf;
    struct netbuf *recvBuf;
    void * recvData;
    uint16_t recvDataLength;
    uint16_t recvNumber;
    err_t err_recv;
    while(1) {
        do {
            buf = netbuf_alloc(netbuf, 0x10);
            if(buf == NULL) {
                printd("couldn't allocate buffer\n");
                return;
            }
            memset(buf, 0, 0x10);
            //memset(buf, 1, 1);
            if(netconn_send(serverConn, netbuf) != ERR_OK) {
                printd("failed to send message\n");
                continue;
            }
            printd("message send\n");
            err_recv = netconn_recv(serverConn, &recvBuf);
        } while(err_recv == ERR_TIMEOUT);
        if(err_recv != ERR_OK) {
            printd("failed to receive msg, err code %d\n", err_recv);
        }
        else {
            printd("received answer\n");
            if(netbuf_data(recvBuf, &recvData, &recvDataLength) != ERR_OK) { //get data from recvBuf
                printd("couldn't receive data from buffer\n");
            }
            else {
                if(((uint8_t *)recvData)[0]-0x30 == 0) {
                    printd("time didn't change, do nothing\n");
                }
                else {
                    recvNumber = 0;
                    for(uint8_t i=1; i<5; i++) {
                        recvNumber *= 10;
                        recvNumber += (((uint8_t *)recvData)[i]-0x30);
                    }
                    printd("time changed: %d\n", recvNumber);
                    xQueueOverwrite(*numberQueue, &recvNumber);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
        netbuf_delete(recvBuf);
    }
}

// param: QueueHandle_t *, queue to write received numbers to
static void time_update_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(5000)); // wait for connection to wlan to be set up
    QueueHandle_t *numberQueue = (QueueHandle_t *) pvParameters;
    if(numberQueue == NULL) {
        printd("received QueueHandle is NULL\n");
        vTaskDelete(NULL);
    }
    time_update(numberQueue);
    uint16_t errorNum = 8888;
    xQueueOverwrite(*numberQueue, &errorNum); // write suspicious number to indicate error
    vTaskDelete(NULL);
}

static void  wifi_task(void *pvParameters) {
    xSemaphoreTake(wifi_alive, portMAX_DELAY);
    uint8_t status  = 0;
    uint8_t retries = 30;
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    printd("WiFi: connecting to WiFi\n\r");
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    while(1)
    {
        while ((status != STATION_GOT_IP) && (retries)){
            status = sdk_wifi_station_get_connect_status();
            printd("%s: status = %d\n\r", __func__, status );
            if( status == STATION_WRONG_PASSWORD ){
                printd("WiFi: wrong password\n\r");
                break;
            } else if( status == STATION_NO_AP_FOUND ) {
                printd("WiFi: AP not found\n\r");
                break;
            } else if( status == STATION_CONNECT_FAIL ) {
                printd("WiFi: connection failed\r\n");
                break;
            }
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
            --retries;
        }
        if (status == STATION_GOT_IP) {
            printd("WiFi: Connected\n\r");
            //xSemaphoreGive( wifi_alive );
            taskYIELD();
        }

        while ((status = sdk_wifi_station_get_connect_status()) == STATION_GOT_IP) {
            //xSemaphoreGive( wifi_alive );
            taskYIELD();
        }
        printd("WiFi: disconnected\n\r");
        sdk_wifi_station_disconnect();
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
}

#endif /* WATCHDOG_NETWORKING_H */
