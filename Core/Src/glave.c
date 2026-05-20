#include "glave.h"
#include "rthw.h"
#include "main.h"
#include "stm32f1xx_hal.h"

void input_monitor(void *keycode) {
    while (1) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(500);
        rt_thread_suspend(rt_thread_self());
    }
}


void glave_main(void *keycode) {
    while (1) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        rt_thread_resume(th_input);
        rt_thread_delay(1000);
    }
}