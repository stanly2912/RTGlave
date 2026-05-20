#ifndef GLAVE_H
#define GLAVE_H

#include "rtthread.h"

extern rt_thread_t th_input, th_main;

void input_monitor(void *keycode);
void glave_main(void *keycode);

#endif