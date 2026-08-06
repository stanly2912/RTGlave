#include "glave.h"
#include "BLE/at_ble.h"
#include "at.h"
#include "protocol.h"


#include "rtdef.h"
#include "rthw.h"
#include "main.h"
#include "rtthread.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_gpio.h"
#include "tb05.h"
#include <stdint.h>
#include <string.h>

#include "max30102_module.h"

#define BLINK_DISC 500
#define BLINK_IDLE 250
#define BLINK_BUSY 70

extern UART_HandleTypeDef huart2;
extern Health_Data_t g_health_data;

static struct rt_mutex key_lock;
static uint32_t blink_time = BLINK_DISC;

static int switch_event(uint8_t cur_func);
static void select_event(uint8_t cur_func);

void input_monitor(void *keycode) {
    int32_t *keycode32 = (int32_t *) keycode;
    volatile uint8_t input_state, prev_state = 0;
    rt_mutex_init(&key_lock, "key_lock", RT_IPC_FLAG_FIFO);
    while (1) {
        input_state = (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) << 1)
                    | !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

        
        if (input_state == prev_state) {
            if (rt_mutex_trytake(&key_lock)) {

                switch (input_state) {
                    case 0: *keycode32 = key_none; break;
                    case 1: *keycode32 = key_switch; break;
                    case 2: *keycode32 = key_select; break;
                    default: *keycode32 = key_undef; break;
                }

                if (*keycode32 != key_undef && *keycode32 != key_none) {
                    rt_thread_delay(200);
                }
                rt_mutex_release(&key_lock);
            }
        }
        else {
            prev_state = input_state;
        }
        rt_thread_delay(40);
    }
}

void led_blink (void *param) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
        rt_thread_delay(blink_time);
    }
}

static uint8_t packbuf[1024];
char remote_mac[2][12];
const uint8_t resp[4] = {SQB_HEAD, SQB_TYPE_RESP, 0, 0};

void glave_main(void *keycode) {
    // Declarations
    uint32_t keycopy = key_none;
    uint8_t cur_func = 0;
    
    // Initialization
    rt_enter_critical();
    tb05_init(0, "RTGlave", BLE_MASTER);
    while (at_wait_ok() != AT_TIMEOUT) {}

    tb05_force_scan(0, "RTHelmet", remote_mac[0]);
    tb05_force_scan(0, "RTTap", remote_mac[1]);
    
    rt_thread_startup(th_input);

    rt_exit_critical();
    
    rt_tick_t check_connection = 0;

    while (1) {
        // Connection state check
        if (rt_tick_get() - check_connection > 1000) {
            int blestate, ret;
            ret = at_blestate(0, &blestate);
            if (ret != AT_OK)
            {
                check_connection = rt_tick_get();
                blink_time = BLINK_DISC;
                continue;
            }
            if (blestate == 0) {
                tb05_force_connect(0, remote_mac[0]);
                at_exit_tranfer(0);
                check_connection = rt_tick_get();
                blink_time = BLINK_IDLE;
            }
            else {
                check_connection = rt_tick_get();
                blink_time = BLINK_IDLE;
            }
        }

        // input parse
        keycopy = *(uint32_t*)keycode;

        switch (keycopy) {
            case key_none: break;
            case key_switch:
                blink_time = BLINK_BUSY;
                at_enter_tranfer(0);
                cur_func += switch_event(cur_func);
                cur_func %= 5;
                at_exit_tranfer(0);
                blink_time = BLINK_IDLE;
                break;
            case key_select:
                blink_time = BLINK_BUSY;
                at_enter_tranfer(0);
                select_event(cur_func);
                at_exit_tranfer(0);
                blink_time = BLINK_IDLE;
                break;
            default: break;
        }

        *(uint32_t*) keycode = key_none;
        // rt_exit_critical();
    }
}


static int switch_event(uint8_t cur_func) {
    int size;
    cur_func = (cur_func + 1) % 5;
    size = sqb_pack(packbuf, SQB_TYPE_SWITCH, sizeof(cur_func), &cur_func);
    at_send(0, packbuf, size);

    // wait for response, return 1 if receive, 0 if no increase
    const uint8_t header = SQB_HEAD;
    uint8_t *rcv = at_wait_for(&header, 1, AT_TIMEOUT_TIME);
    if (rcv != NULL) {
        uint32_t begin = rt_tick_get();
        while (at_readable_len() < 4) {
            if (rt_tick_get() - begin > AT_TIMEOUT_TIME) {
                break;
            }
        }

        if (at_readable_len() >= 4) {
            memcpy(packbuf, rcv, 4);
            at_pop(4);
            if (sqb_type(packbuf) == SQB_TYPE_RESP) {
                at_pop(at_readable_len());
                return 1;
            }
        }
    }
    
    at_pop(at_readable_len());
    return 0;
}

static void select_event(uint8_t cur_func) {
    
    switch (cur_func) {
        case 0: // 疲劳检测
        {
            int ret, size;
            at_exit_tranfer(0);

            ret = tb05_disconnect(0);
            if (ret != AT_OK) return;

            ret = tb05_connect(0, remote_mac[1]); // 更改连接到 remote_mac[1]
            if (ret != AT_OK) return;

            uint8_t bd = 0;
            size = sqb_pack(packbuf, SQB_TYPE_SELECT, 1, &bd);
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
                            tb05_disconnect(0);
                            ret = tb05_connect(0, remote_mac[0]);
                            if (ret == AT_TIMEOUT) {
                                uint8_t *p;
                                int wait = 5;
                                while (wait--) {
                                    p = at_wait_for((const uint8_t *)"+EVENT", 6, AT_TIMEOUT_TIME);
                                    if (p != NULL) {
                                        ret = AT_OK;
                                        break;
                                    }
                                }
                                if (wait == -1) {
                                    tb05_disconnect(0);
                                    at_pop(at_readable_len());
                                    break;
                                }
                            }
                            
                            uint32_t t = *(uint32_t*) sqb_body(packbuf);
                            size = sqb_pack(packbuf, SQB_TYPE_DATA, 4, (uint8_t *)&t);
                            HAL_Delay(1000);
                            at_send(0, packbuf, size);
                        }
                        else {
                            at_pop(at_readable_len());
                        }
                    }
                }
                
                tb05_disconnect(0);
            }
        }
        break;
        case 1: // 心率
        case 2: // 血氧
        case 4: // 卡路里
        {
            /*
                * 心率、血氧、卡路里按触点功能分别判断有效性：
                * case 1 心率 -> hr_valid，并发送心率预警码 0/1/2
                * case 2 血氧 -> spo2_valid，并发送血氧预警码 0/3
                * case 4 卡路里 -> hr_valid，预警码固定为 0
                */
            {
                uint8_t selected_valid = 0U;
                uint32_t selected_alert_code = 0U;

                if (cur_func == 1U)
                {
                    selected_valid = (uint8_t)g_health_data.hr_valid;
                    selected_alert_code = g_health_data.hr_alert_code;
                }
                else if (cur_func == 2U)
                {
                    selected_valid = (uint8_t)g_health_data.spo2_valid;
                    selected_alert_code = g_health_data.spo2_alert_code;
                }
                else if (cur_func == 4U)
                {
                    selected_valid = (uint8_t)g_health_data.hr_valid;
                    selected_alert_code = 0U;
                }

                if (selected_valid != 0U)
                {
                    static int32_t spo2 = 97;
                    static uint32_t start_time;

                    if (start_time == 0) {
                        start_time = HAL_GetTick();
                    }

                    if (g_health_data.spo2 != 0) {
                        spo2 = g_health_data.spo2;
                    }

                    health_data_t dat = {
                        g_health_data.heart_rate,
                        spo2,
                        (g_health_data.kcal_x100 + 50U) / 1000U,
                        (HAL_GetTick() - start_time) / 1000,
                        selected_alert_code
                    };
                    int size = sqb_pack(packbuf, SQB_TYPE_DATA, sizeof(dat), (const uint8_t *)&dat);
                    at_send(0, packbuf, size);
                }
            }
            
            // size = sqb_pack(packbuf, SQB_TYPE_DATA, sizeof(dat), (const uint8_t *)&dat);
            // at_send(0, packbuf, size);
            

            int size = sqb_pack(packbuf, SQB_TYPE_SELECT, sizeof(cur_func), &cur_func);
            at_send(0, packbuf, size);
        }
        break;
        case 3:
        {
            int ret, size;
            ret = tb05_disconnect(0);
            if (ret != AT_OK) return;

            ret = tb05_connect(0, remote_mac[1]);//改为 remote_mac[1]
            if (ret != AT_OK) return;
            
            uint8_t bd = 0;
            size = sqb_pack(packbuf, SQB_TYPE_SELECT, 1, &bd);
            at_send(0, packbuf, size);
            tb05_disconnect(0);
        }
        break;
        default: break;
    }
}