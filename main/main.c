#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define X_AXIS_CHANNEL 26
#define Y_AXIS_CHANNEL 27

QueueHandle_t xQueueAdc;

typedef struct adc {
    int axis;
    int val;
} adc_t;

void adc_setup() {
    adc_init();
    adc_gpio_init(X_AXIS_CHANNEL);
    adc_gpio_init(Y_AXIS_CHANNEL);
}

void write_package(adc_t data) {
    int val = data.val;
    int msb = val >> 8;
    int lsb = val & 0xFF ;

    uart_putc_raw(uart0, data.axis);
    uart_putc_raw(uart0, lsb);
    uart_putc_raw(uart0, msb);
    uart_putc_raw(uart0, -1);
}

int read_and_scale_adc(int axis) {
    int sum = 0;
    adc_select_input(axis);
    for (int i = 0; i < 5; i++) {
        sum += adc_read();
    }
    int avg = sum / 5;

    int scaled_val = ((avg - 2048) / 8);

    if ((scaled_val > -180) && (scaled_val < 180)) {
        scaled_val = 0; // Apply deadzone
    }

    return scaled_val / 16;
}

void x_task(void *p) {
    adc_t data;
    data.axis = 0; // X-axis

    while (1) {
        data.val = read_and_scale_adc(data.axis);
        xQueueSend(xQueueAdc, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void y_task(void *p) {
    adc_t data;
    data.axis = 1; // Y-axis

    while (1) {
        data.val = read_and_scale_adc(data.axis);
        xQueueSend(xQueueAdc, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void uart_task(void *p) {
    adc_t data;
    
    while (1) {
        if (xQueueReceive(xQueueAdc, &data, portMAX_DELAY)) {
            write_package(data);
        }
    }
}

int main() {
    stdio_init_all();
    adc_setup();

    xQueueAdc = xQueueCreate(32, sizeof(adc_t));

    xTaskCreate(x_task, "X Task", 256, NULL, 1, NULL);
    xTaskCreate(y_task, "Y Task", 256, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) ;
}
