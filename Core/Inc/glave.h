#ifndef GLAVE_H
#define GLAVE_H

#include "rtthread.h"
#include <stdint.h>

typedef enum glave_key { key_none = 0, key_change = 1, key_select = 2, key_undef = -1} glave_key;

extern rt_thread_t th_input, th_main;

void input_monitor(void *keycode);
void glave_main(void *keycode);

#endif