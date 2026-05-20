#include "at.h"

#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"
#include "usart.h"

#include <stdint.h>

#include "rtthread.h"
#include "rtdef.h"

extern int at_memcmp(const uint8_t *buf1, const uint8_t *buf2, int size);

static int offset, finish;
static uint8_t *current_line;

static int connected = 0;

void at_connected_set(int state) {
    connected = state;
    if (state) {
        offset = 0;
        finish = 0;
        current_line = at_receive_buf;
        for (int i = 0; i < AT_LINE_SIZE * AT_LINE_NUM_MAX; i++) {
            current_line[i] = 0;
        }
    }
}
int at_connected_get(void) {
    return connected;
}

void at_delay(uint32_t ms) {
    rt_thread_delay((rt_tick_t) ms);
}

int at_send(int dev_id, const uint8_t *data, uint32_t size) {
    int ret;
    UART_HandleTypeDef *huart;

    switch (dev_id) {
        case 0: 
            huart = &huart4;
            break;
        case 1: 
            huart = &huart5;
            break;
        default:
            return AT_ERR;
    }

    ret = HAL_UART_Transmit(huart, (const uint8_t *)data, size, at_gettimeout());

    if (ret == HAL_OK) {
        return AT_OK;
    }
    else if (ret == HAL_TIMEOUT)
        return AT_TIMEOUT;
    else
        return AT_ERR;
}


int at_receive_line(int dev_id, uint8_t *line) {
    UART_HandleTypeDef *huart;

    switch (dev_id) {
        case 0: 
            huart = &huart4;
            break;
        case 1: 
            huart = &huart5;
            break;
        default:
            return AT_ERR;
    }

    current_line = line;
    offset = finish = 0;
    HAL_UART_Receive_IT(huart, line, 1);
    uint32_t target = HAL_GetTick() + at_gettimeout();
    while (HAL_GetTick() < target) {
        if (finish == 0) {
            continue;
        }
        else if (finish == 1) {
            return AT_OK;
        }
        else {
            return AT_ERR;
        }
    }
    HAL_UART_AbortReceive(huart);
    return AT_TIMEOUT;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (at_connected_get()) {
        offset += 1;
        if (offset < AT_LINE_SIZE * AT_LINE_NUM_MAX) {
            HAL_UART_Receive_IT(huart, current_line + offset, 1);
        }
    }
    else if (huart->Instance == UART4 || huart->Instance == UART5) {
        if (current_line[offset] == '\n') {
            finish = 1;
        }
        else {
            offset += 1;
            if (offset == AT_LINE_SIZE) {
                finish = 2;
            }
            else {
                HAL_UART_Receive_IT(huart, current_line + offset, 1);
            }
        }
    }
}