#include "tb05.h"
#include "at.h"
#include "BLE/at_ble.h"

#include <stdbool.h>

#include "main.h"
#include "protocol.h"

volatile uint32_t tb05_rp, tb05_wp;
volatile bool tb05_full;

extern UART_HandleTypeDef huart2
;

void tb05_init(int dev_id, const char *name, AT_BLE_Mode mode) {
    TB05_HW_RST();
    int ret = 1;
		volatile int dbgret;
    while ((dbgret = at_rst(dev_id)) != AT_OK) {
        TB05_BLINK(ret);
				TB05_HW_RST();
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
            *data = at_receive_buf[tb05_rp % (AT_LINE_SIZE*AT_LINE_NUM_MAX)];
            tb05_rp = (tb05_rp + 1);
        }
        else {
            break;
        }
    }
		if (tb05_full) {
			// 从满状态恢复
			tb05_full = false;
			HAL_UART_Receive_IT(&huart2, &at_receive_buf[tb05_wp], 1);
		}
    return cur_size;
}

int tb05_read_blocking(int dev_id, uint8_t *data, int size) {
    int ret = size;
    while (size > 0) {
        size -= tb05_read(dev_id, data, size);
    }
    return ret;
}

int tb05_write(int dev_id, const uint8_t *data, int size) {
    int ret = at_send(dev_id, data, size);
    if (ret == AT_OK) {
        return size;
    }
    else {
        return -1;
    }
}

int tb05_listen(int dev_id) {
	if (at_connected_get() == 0) {
		at_connected_set(1);
		return 1;
	}
	return 0;
}