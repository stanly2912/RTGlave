#ifndef AT_H
#define AT_H

#include <stdint.h>

typedef enum AT_State { AT_IDLE, AT_SEND, AT_RECEIVE } AT_State;
typedef enum AT_Return { AT_OK, AT_ERR, AT_TIMEOUT} AT_Return;

#define AT_TIMEOUT_TIME 450
#define AT_SCAN_TIME 2500
#define AT_LINE_SIZE 32
#define AT_LINE_NUM_MAX 16

extern uint8_t at_receive_buf[AT_LINE_SIZE * AT_LINE_NUM_MAX];

/* This 2 function may be used in your porting */
uint32_t at_gettimeout(void);
void at_settimeout(uint32_t time);

/* port functions */
#define AT_USER_INC "rtthread.h"
#define AT_Log(FMT, ...) rt_kprintf("[AT]: " FMT "\n", ##__VA_ARGS__)

void at_delay(uint32_t ms);
int at_send(int dev_id, const uint8_t *data, uint32_t size);
int at_receive_line(int dev_id, uint8_t *line);
/* end of port functions*/

/* internal functions */
int at_sendseq(int dev_id, ...);
int at_receive(int dev_id);
int at_memcmp(const uint8_t *buf1, const uint8_t *buf2, int size);
int at_ok_parse(const uint8_t *buf, int size);
int at_strcmp(const char *str1, const char* str2);
int at_strlen(const char *str);

/* common function */
AT_Return at_rst(int dev_id);

#endif
