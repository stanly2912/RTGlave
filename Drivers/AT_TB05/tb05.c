#include "tb05.h"
#include "BLE/at_ble.h"
#include "at.h"
#include "stm32f1xx_hal_uart.h"
#include <stdbool.h>

volatile uint32_t tb05_rp, tb05_wp;
volatile bool tb05_full;

extern UART_HandleTypeDef huart2;

void tb05_init(int dev_id, const char *name, AT_BLE_Mode mode) {
    TB05_HW_RST();
    int ret = 1;
    while (at_rst(dev_id) != AT_OK) {
        TB05_BLINK(ret);
        ret = !ret;
    }
    TB05_BLINK(0);

    ret = at_blename_set(dev_id, name, 0);
    TB05_BLINK(ret);

    ret = at_blemode_set(dev_id, mode);
    TB05_BLINK(ret);

    while (ret != AT_OK) ;
}

int tb05_read(int dev_id, uint8_t *data, int size) {
    int cur_size = 0;
    while (cur_size < size) {
        if (tb05_rp != tb05_wp) {
            cur_size += 1;
            *data = at_receive_buf[tb05_rp];
            tb05_rp = (tb05_rp + 1) % (AT_LINE_SIZE * AT_LINE_NUM_MAX);
            if (tb05_full) {
                // 从满状态恢复
                tb05_full = false;
                HAL_UART_Receive_IT(&huart2, &at_receive_buf[tb05_wp], 1);
            }
        }
        else {
            break;
        }
    }
    return cur_size;
}

int tb05_write(int dev_id, const uint8_t *data, int size) {
    at_send(dev_id, data, size);
}

int tb05_connect(int dev_id, const char *name) {
    char mac[12];
    int ret = at_blescan(dev_id, name, mac);
    if (ret == AT_OK) {
        ret = at_bleconnect(dev_id, mac);
    }
    return ret;
}

void tb05_force_connect(int dev_id, const char *name) {
    while (tb05_connect(dev_id, name) != AT_OK);
}

int tb05_listen(int dev_id) {
    return 0;
}