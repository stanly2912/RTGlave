#include "glave.h"
#include "BLE/at_ble.h"
#include "at.h"
#include "protocol.h"


#include "rthw.h"
#include "main.h"
#include "rtthread.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_def.h"
#include "tb05.h"
#include <stdint.h>
#include <string.h>

extern UART_HandleTypeDef huart2;

void input_monitor(void *keycode) {
    int32_t *keycode32 = (int32_t *) keycode;
    volatile uint8_t input_state, prev_state = 0;
    while (1) {
        input_state = (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) << 1)
                    | !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);
        if (input_state == prev_state) {
            switch (input_state) {
                case 0: *keycode32 = key_none; break;
                case 1: *keycode32 = key_switch; break;
                case 2: *keycode32 = key_select; break;
                default: *keycode32 = key_undef; break;
            }

            if (*keycode32 != key_undef && *keycode32 != key_none) {
                rt_thread_delay(200);
            }
        }
        else {
            prev_state = input_state;
        }
        rt_thread_delay(20);
    }
}

static uint8_t packbuf[1024];
const uint8_t resp[4] = {SQB_HEAD, SQB_TYPE_RESP, 0, 0};

void glave_main(void *keycode) {
    // Declarations
    uint32_t keycopy = key_none;
    uint8_t cur_func = 0;
    char remote_mac[2][12];
    
    // Initialization
    rt_enter_critical();
    tb05_init(0, "RTGlave", BLE_MASTER);
    
    while (at_wait_ok() != AT_TIMEOUT) {}
    tb05_force_scan(0, "RTHelmet", remote_mac[0]);
    tb05_force_scan(0, "RTTap", remote_mac[1]);

    while (tb05_connect(0, remote_mac[0]) != AT_OK) {
        tb05_disconnect(0);
        rt_thread_delay(1000);
    }

    rt_exit_critical();
    
    rt_thread_startup(th_input);

    while (1) {
        rt_enter_critical();
        keycopy = *(uint32_t *) keycode;
        

        switch (keycopy) {
            case key_none: break;
            case key_switch: {
                    cur_func += 1;
                    if (cur_func > 2) {
                        cur_func = 0;
                    }

                    int size = sqb_pack(packbuf, SQB_TYPE_SWITCH, sizeof(cur_func), &cur_func);
                    int ret, blestate;
                    at_exit_tranfer(0);
                    ret = at_blestate(0, &blestate);
                    if (ret != AT_OK) break;
                    if (blestate == 0) {
                        at_bleconnect(0, remote_mac[0]);
                    }
                    else {
                        at_enter_tranfer(0);
                    }
                    at_send(0, packbuf, size);
                }
                break;
            case key_select:
                switch (cur_func) {
                    case 0: // 疲劳检测
                    {
                        int ret, blestate, size;
                        at_exit_tranfer(0);
                        ret = at_blestate(0, &blestate);
                        if (ret != AT_OK) break;
                        if (blestate != 0) {
                            tb05_disconnect(0);
                        }
                        ret = at_bleconnect(0, remote_mac[1]);//改为 remote_mac[1]
                        if (ret == AT_OK) {
                            size = sqb_pack(packbuf, SQB_TYPE_DATA, 0, NULL);
                            at_send(0, packbuf, size);
                            const uint8_t header = SQB_HEAD;
                            uint8_t *rcv = at_wait_for(&header, 1, AT_TIMEOUT_TIME);
                            if (rcv != NULL) {
                                uint32_t begin = HAL_GetTick();
                                while (at_readable_len() < 4) {
                                    if (HAL_GetTick() - begin > AT_TIMEOUT_TIME) {
                                        at_pop(at_readable_len());
                                        break;
                                    }
                                }
                                if (at_readable_len() >= 4) {
                                    memcpy(packbuf, rcv, 4);
                                    at_pop(4);
                                    if (sqb_type(packbuf) == SQB_TYPE_RESP) {
                                        while (at_readable_len() < sqb_bodylen(packbuf)) {
                                            if (HAL_GetTick() - begin > AT_TIMEOUT_TIME) {
                                                at_pop(at_readable_len());
                                                break;
                                            }
                                        }
                                        if (at_readable_len() >= sqb_bodylen(packbuf)) {
                                            memcpy(sqb_body(packbuf), at_readptr(), sqb_bodylen(packbuf));
                                            at_pop(sqb_bodylen(packbuf));

                                            /* 开始语音播报 */
                                            uint8_t body[2] = {0, sqb_body(packbuf)[0]};
                                            size = sqb_pack(packbuf, SQB_TYPE_SELECT, sizeof body, body);
                                            tb05_disconnect(0);
 
                                            at_bleconnect(0, remote_mac[0]);
                                        }
                                        else {
                                            at_pop(at_readable_len());
                                        }
                                    }
                                }
                            }
                            tb05_disconnect(0);
                        }
                    }
                    break;
                    case 1: // 心率
                    case 2: // 血氧
                    {
                        int ret, blestate, size;
                        size = sqb_pack(packbuf, SQB_TYPE_SELECT, sizeof(cur_func), &cur_func);
                        at_exit_tranfer(0);
                        ret = at_blestate(0, &blestate);
                        if (ret != AT_OK) break;
                        if (blestate == 0) {
                            at_bleconnect(0, remote_mac[0]);
                        }
                        else {
                            at_enter_tranfer(0);
                        }
                        at_send(0, packbuf, size);
                    }
                    default: break;
                }
                break;
            default: break;
        }

        *(uint32_t*) keycode = key_none;
        rt_exit_critical();
    }
}