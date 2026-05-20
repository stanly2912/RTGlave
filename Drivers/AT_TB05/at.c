#include "at.h"
#include <stdarg.h>
#include <stdint.h>
#include AT_USER_INC

uint8_t at_receive_buf[AT_LINE_SIZE * AT_LINE_NUM_MAX];
static uint32_t at_time_out = AT_TIMEOUT_TIME;

uint32_t at_gettimeout(void) {
    return at_time_out;
}
void at_settimeout(uint32_t time) {
    at_time_out = time;
}

int at_receive(int dev_id) {
    
    for (int i = 0; i < AT_LINE_NUM_MAX; i++) {
        uint8_t *line = at_receive_buf + (i * AT_LINE_SIZE);
        int ret = at_receive_line(dev_id, line);
        if (ret == AT_OK) {
            if (at_memcmp(line, (const uint8_t *)"OK\r\n", 4) == 0) {
                while (at_receive_line(dev_id, line) != AT_TIMEOUT);
                return AT_OK;
            }
            else if (at_memcmp(line, (const uint8_t *)"ERROR\r\n" , 7) == 0) {
                return AT_ERR;
            }
        }
        else
            return ret;
    }

    return AT_ERR;
}

int at_sendseq(int dev_id, ...) {
    va_list seqs;
    const char *seq;
    int ret;
    va_start(seqs, dev_id);
    do {
        seq = va_arg(seqs, const char *);
        ret = at_send(dev_id, (const uint8_t*)seq, at_strlen(seq));
        if (ret != AT_OK) {
            break;
        }
    }
    while (at_strcmp(seq, "\r\n") != 0);
    va_end(seqs);
    return ret;
}

int at_strcmp(const char *str1, const char* str2) {
    while (*str1 || *str2) {
        if (*str1 < *str2) {
            return -1;
        }
        else if (*str1 > *str2) {
            return 1;
        }
        str1++;
        str2++;
    }
    return 0;
}

int at_memcmp(const uint8_t *buf1, const uint8_t *buf2, int size) {
    for (int i = 0; i < size; i++) {
        if (buf1[i] < buf2[i]) {
            return -1;
        }
        else if (buf1[i] > buf2[i]) {
            return 1;
        }
    }
    return 0;
}

int at_strlen(const char *str) {
    int i = 0;
    while (str[i]) {
        i += 1;
    }
    return i;
}

AT_Return at_rst(int dev_id) {
    AT_Return ret;
    ret = at_send(dev_id, (const uint8_t *) "AT+RST\r\n", at_strlen("AT+RST\r\n"));
    if (ret != AT_OK) {
        return ret;
    }
    ret = at_receive(dev_id);
    return ret;
}
