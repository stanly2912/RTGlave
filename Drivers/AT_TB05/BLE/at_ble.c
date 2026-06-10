#include "at_ble.h"
#include <stdint.h>

int at_blename_set(int dev_id, const char *name, bool save_flash) {
    int ret = at_sendseq(dev_id, "AT+BLENAME=", name, "\r\n");
    if (ret != AT_OK) {
        return ret;
    }
    return at_receive(dev_id);
}

int at_blename_get(int dev_id, char *name_buf) {
    int ret;
    ret = at_send(dev_id, (const uint8_t *)"AT+BLENAME?\r\n", at_strlen("AT+BLENAME?\r\n"));
    if (ret != AT_OK) {
        return ret;
    }
    return at_receive(dev_id);
}

int at_blemode_set(int dev_id, int mode) {
    const char args[] = {mode + '0', '\0'};
    int ret = at_sendseq(dev_id, "AT+BLEMODE=", args, "\r\n");
    if (ret != AT_OK) {
        return ret;
    }
    return at_receive(dev_id);
}

int at_bleauth_set(int dev_id, const char *code) {
    int ret = at_sendseq(dev_id, "AT+BLEAUTH=", code, "\r\n");
    if (ret != AT_OK) return ret;
    return at_receive(dev_id);
}

int at_bleadven_set(int dev_id, bool en) {
    const char status[] = {en + '\0', '\0'};
    int ret = at_sendseq(dev_id, "AT+BLEADVEB=", status, "\r\n");
    if (ret != AT_OK) return ret;
    return at_receive(dev_id);
}

int at_blescan(int dev_id, const char *target_name, char mac[12]) {
    uint8_t *line = at_receive_buf;
    int stage = 0;
    int debug_count = 0;
    const char *tag = "OK\r\n";
    at_settimeout(AT_SCAN_TIME);
    int len = at_strlen(tag);
    int ret = at_send(dev_id, (const uint8_t *) "AT+BLESCAN\r\n", at_strlen("AT+BLESCAN\r\n"));
    if (ret != AT_OK) {
        goto Ret;
    }
    while ((ret = at_receive_line(dev_id, line)) == AT_OK) {
        switch (stage) {
            case 0:
            if (at_memcmp(line, (const uint8_t *) tag, len) == 0) {
                stage += 1;
                tag = "name:";
                len = at_strlen(tag);
            }
            break;
            case 1:
            if (at_memcmp(line, (const uint8_t *) tag, len) == 0) {
                if (at_memcmp(line + len, (const uint8_t *)target_name, at_strlen(target_name)) == 0) {
                    stage += 1;
                    tag = "MAC:";
                    len = at_strlen(tag);
                }
            }
            break;
            case 2:
            if (at_memcmp(line, (const uint8_t *) tag, len) == 0) {
                for (int i = 0; i < 12; i++) {
                    mac[i] = (line + len)[i];
                }
                ret = AT_OK;
                goto Ret;
            }
            default: 
            ret = AT_ERR;
            goto Ret;
        }
        line += AT_LINE_SIZE;
        debug_count++;
        if (line - at_receive_buf >= AT_LINE_SIZE * AT_LINE_NUM_MAX) {
            line = at_receive_buf;
        }
    }
    ret = stage != 0 ? ret : AT_ERR;

    Ret:
    at_settimeout(AT_TIMEOUT_TIME);
    while (at_receive_line(dev_id, line) != AT_TIMEOUT) ;
    return ret;
}

int at_bleconnect(int dev_id, const char mac[12]) {
    at_sendseq(dev_id, "AT+BLECONNECT=", mac, "\r\n");
    return at_receive(dev_id);
}

int at_exit_tranfer(int dev_id) {
    const uint8_t cmd [] = "+++";
    at_send(dev_id, (const uint8_t *)cmd, at_strlen((const char *)cmd));
    return at_receive(dev_id);
}

int at_blediscon(int dev_id) {
    const uint8_t cmd [] = "AT+BLEDISCON\r\n";
    at_send(dev_id, cmd, at_strlen((const char*)cmd));
    return at_receive(dev_id);
}

int at_blestate(int dev_id, int *state) {
    const uint8_t cmd [] = "AT+BLESTATE?\r\n";
    uint8_t *line = at_receive_buf;

    // at_settimeout(AT_SCAN_TIME);
    at_send(dev_id, cmd, at_strlen((const char*)cmd));

    int cnt = 0;
    int ret;
    do {
        ret = at_receive_line(dev_id, line);
        if (ret == AT_ERR) {
            cnt = 0;
            break;
        }
        cnt += 1;
        line += AT_LINE_SIZE;
    } while (ret != AT_TIMEOUT);

    const char *tag1 = "OK\r\n", *tag2 = "+";
    for (int i = 0; i < cnt; i++) {
        line = at_receive_buf + i * AT_LINE_SIZE;
        if ( at_memcmp(line, (const uint8_t *) tag1, at_strlen(tag1)) == 0) {
            ret = AT_OK;
        }
        else if ( at_memcmp(line, (const uint8_t *) tag2, at_strlen(tag2)) == 0) {
            *state = line[10] - '0';
        }
    }

    // at_settimeout(AT_TIMEOUT_TIME);
    return ret;
}