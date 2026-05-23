#ifndef AT_BLE_H
#define AT_BLE_H

#include "../at.h"
#include <stdbool.h>

typedef enum AT_BLE_Mode {BLE_SLAVE = 0, BLE_MASTER = 1, BLE_IBEACON = 2, BLE_OFF = 9} AT_BLE_Mode;

void at_connected_set(int state);
int at_connected_get(void);

int at_blename_set(int dev_id, const char *name, bool save_flash);

/* Incompleted */
int at_blename_get(int dev_id, char *name_buf);

int at_blemode_set(int dev_id, int mode);

int at_blediscon(int dev_id);

int at_blestate(int dev_id, int *state);

/* 从机命令 */

int at_bleadven_set(int dev_id, bool en);

/* 使用 DISENABLE 禁用 pin */
int at_bleauth_set(int dev_id, const char *code);

/* 主机命令 */

/* 如果扫描到目标，返回 AT_OK，未找到返回AT_TIMEOUT，扫描发生错误返回AT_ERR */
int at_blescan(int dev_id, const char *target_name, char mac[12]);
int at_bleconnect(int dev_id, const char mac[12]);

#endif
