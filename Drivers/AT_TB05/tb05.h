#ifndef TB05_H
#define TB05_H

#include "at.h"
#include "BLE/at_ble.h"

#define TB05_HW_RST() do { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); HAL_Delay(10); HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);} while(0)
#define TB05_BLINK(L)

// Common fucntions

/* 复位模块，初始化蓝牙功能 */
void tb05_init(int dev_id, const char *name, AT_BLE_Mode mode);

/* 读/写信道，返回读/写数量，实际读写可能小于预期值 */
int tb05_read(int dev_id, uint8_t *data, int size);
int tb05_write(int dev_id, const uint8_t *data, int size);

/* 阻塞，直到所有读/写完成 */
int tb05_read_blocking(int dev_id, uint8_t *data, int size);

/* 断开连接 */
static inline int tb05_disconnect(int dev_id) {
    return at_blediscon(dev_id);
}


// Master functions

/* 发起一次扫描 */
static inline int tb05_scan(int dev_id, const char *name, char mac[12]) {
    return at_blescan(dev_id, name, mac);
}

/* 持续扫描，直到发现目标 */
static inline void tb05_force_scan(int dev_id, const char *name, char mac[12]) {
    while (tb05_scan(dev_id, name, mac) != AT_OK) ;
}

/* 为主机发起连接 */
static inline int tb05_connect(int dev_id, const char mac[12]) {
    return at_bleconnect(dev_id, mac);
}
/* 持续发起连接，直到成功连接 */
static inline void tb05_force_connect(int dev_id, const char mac[12]) {
    while (tb05_connect(dev_id, mac) != AT_OK);
}


// Slave functions

/* 作为从机监听连接，阻塞直到连接建立 */
int tb05_listen(int dev_id);

#endif