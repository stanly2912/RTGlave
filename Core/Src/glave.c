#include "glave.h"
#include "BLE/at_ble.h"
#include "protocol.h"


#include "rthw.h"
#include "main.h"
#include "rtthread.h"
#include "tb05.h"
#include <stdint.h>

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

static uint8_t packetbuf[1024];

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
                    int size = sqb_pack(packetbuf, SQB_TYPE_SWITCH, sizeof(cur_func), &cur_func);
                    tb05_force_connect(0, remote_mac[0]);
                    tb05_disconnect(0);
                }
                break;
            case key_select:
                switch (cur_func) {
                    case 0: // 疲劳检测
                    break;
                    case 1: // 心率
                    case 2: // 血氧

                    default: break;
                }
                break;
            default: break;
        }

        *(uint32_t*) keycode = key_none;
        rt_exit_critical();
    }
}