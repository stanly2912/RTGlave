#include "glave.h"
#include "BLE/at_ble.h"
#include "rthw.h"
#include "main.h"
#include "tb05.h"
#include <stdint.h>

void input_monitor(void *keycode) {
    int32_t *keycode32 = (int32_t *) keycode;
    uint8_t input_state, prev_state = 0;
    while (1) {
        input_state = (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) << 1)
                    | !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);
        if (input_state == prev_state) {
            switch (input_state) {
                case 0: *keycode32 = key_none; break;
                case 1: *keycode32 = key_change; break;
                case 2: *keycode32 = key_select; break;
                default: *keycode32 = key_undef; break;
            }

            if (*keycode32 != key_undef && *keycode32 != key_none) {
                rt_thread_resume(th_main);
                rt_thread_delay(200);
            }
        }
        else {
            prev_state = input_state;
        }
        rt_thread_delay(20);
    }
}


void glave_main(void *keycode) {
    // Initialization
    tb05_init(0, "RTGlave", BLE_MASTER);

    while (1) {
        rt_thread_suspend(rt_thread_self());
        if (*(int32_t *) keycode == key_select) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            *(int32_t *) keycode = key_none;
        }
    }
}