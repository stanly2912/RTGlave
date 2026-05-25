#ifndef GLAVE_H
#define GLAVE_H

#include "tb05.h"
#include "rtthread.h"
#include <stdint.h>

typedef enum glave_key { key_none = 0, key_switch = 1, key_select = 2, key_undef = -1} glave_key;

extern rt_thread_t th_input, th_main;

void input_monitor(void *keycode);
void glave_main(void *keycode);

#endif