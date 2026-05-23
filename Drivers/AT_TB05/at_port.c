#include "at.h"
#include <stdbool.h>
#include <stdint.h>
#include "main.h"
#include "rtthread.h"
#include "rtdef.h"
#include "stm32f1xx_hal_uart.h"

extern UART_HandleTypeDef huart2;
extern volatile uint32_t tb05_rp, tb05_wp;
extern volatile bool tb05_full;

static int offset, finish;
static uint8_t *current_line;

static int connected = 0;

void at_connected_set(int state) {
    connected = state;
    if (state) {
        tb05_rp = 0;
        tb05_wp = 0;
        tb05_full = false;
        HAL_UART_Receive_IT(&huart2, at_receive_buf + tb05_wp, 1);
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
            huart = &huart2;
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
            huart = &huart2;
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
    static int prev_rp;
    if (at_connected_get()) {
        tb05_wp = (tb05_wp + 1) % (AT_LINE_SIZE * AT_LINE_NUM_MAX)
        if (tb05_wp != (tb05_rp - 1) % (AT_LINE_SIZE * AT_LINE_NUM_MAX))
            HAL_UART_Receive_IT(&huart2, at_receive_buf + tb05_wp, 1);
        else
            tb05_full = true;
    }
    else if (huart->Instance == USART2) {
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