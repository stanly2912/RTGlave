#ifndef __MAX30102_MODULE_H__
#define __MAX30102_MODULE_H__

/*
 * MAX30102 模块封装头文件
 *
 * 作用：
 * 1. 把 MAX30102 底层驱动再次封装成“模块”形式。
 * 2. 结构体里统一放：数据成员 + 3 个函数接口。
 * 3. 对上层 main.c 来说，只需要操作这个模块结构体即可。
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "max30102.h"

/*
 * 所有对外数据统一使用 int。
 * 这样做的好处是：
 * 1. 上层调用时不用关心 int32_t / uint32_t 等底层类型。
 * 2. 比赛里答辩时更容易说明“模块接口统一为 int 型”。
 */

/* int 最大值 */
#define MAX30102_MODULE_MAX_INT          ((int)2147483647)
/* int 最小值 */
#define MAX30102_MODULE_MIN_INT          ((int)(-2147483647 - 1))
/* 无效默认值，这里用 0 表示无效 */
#define MAX30102_MODULE_INVALID_VALUE    ((int)0)
/* 有效标志 */
#define MAX30102_MODULE_VALID_FLAG       ((int)1)
/* 无效标志 */
#define MAX30102_MODULE_INVALID_FLAG     ((int)0)
/* 函数执行成功 */
#define MAX30102_MODULE_OK               ((int)0)
/* 函数执行失败 */
#define MAX30102_MODULE_ERROR            ((int)-1)

/* MAX30102 数据有效范围。
 * 这里用于模块内部的基础有效性判断。
 * 更细的预警等级在 main.c 中完成。
 */
#define MAX30102_MODULE_HR_MIN           ((int)40)
#define MAX30102_MODULE_HR_MAX           ((int)220)
#define MAX30102_MODULE_SPO2_MIN         ((int)70)
#define MAX30102_MODULE_SPO2_MAX         ((int)100)

/* 前向声明，先告诉编译器有这么一个结构体类型 */
struct MAX30102_Module;
/* 给结构体类型起别名，后面用 MAX30102_Module_t 更方便 */
typedef struct MAX30102_Module MAX30102_Module_t;

/*
 * 下面 3 个 typedef 是函数指针类型定义。
 * 正好对应 3 个函数：
 * 1. Init     -> 初始化模块
 * 2. Measure  -> 测量传感器数据
 * 3. GetValue -> 返回当前心率、血氧、有效标志
 */
typedef int (*MAX30102_Init_Func)(MAX30102_Module_t *self);
typedef int (*MAX30102_Measure_Func)(MAX30102_Module_t *self);
typedef int (*MAX30102_GetValue_Func)(MAX30102_Module_t *self,
                                       int *heart_rate,
                                       int *spo2,
                                       int *hr_valid,
                                       int *spo2_valid);

/*
 * MAX30102 模块结构体
 *
 * 里面放两类东西：
 * 1. 数据成员
 * 2. 3 个函数接口
 */
struct MAX30102_Module
{
    /* 当前心率值，单位 bpm */
    int heart_rate;
    /* 当前血氧值，单位 % */
    int spo2;
    /* 心率是否有效：1 有效，0 无效 */
    int hr_valid;
    /* 血氧是否有效：1 有效，0 无效 */
    int spo2_valid;
    /* 兼容旧接口：任意一项有效时为 1；上层应优先读取各自 valid */
    int valid;
    /* 当前模块是否已经初始化：1 已初始化，0 未初始化 */
    int is_init;
    /* 记录 int 型最大值，满足“宏定义最大变量，int 型”的要求 */
    int max_value;

    /* 初始化函数指针 */
    MAX30102_Init_Func Init;
    /* 测量函数指针 */
    MAX30102_Measure_Func Measure;
    /* 取值函数指针 */
    MAX30102_GetValue_Func GetValue;
};

/*
 * 创建模块对象
 *
 * 作用：
 * 1. 给结构体清零。
 * 2. 初始化默认值。
 * 3. 把 3 个函数实现绑定到结构体里。
 *
 * 调用后可以这样用：
 *   module.Init(&module);
 *   module.Measure(&module);
 *   module.GetValue(&module, &hr, &spo2, &hr_valid, &spo2_valid);
 */
void MAX30102_Module_Create(MAX30102_Module_t *self);

#ifdef __cplusplus
}
#endif

#endif /* __MAX30102_MODULE_H__ */
