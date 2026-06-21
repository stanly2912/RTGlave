#include "max30102_module.h"
#include <string.h>

/*
 * 内部初始化函数实现
 *
 * 说明：
 * 1. 这是结构体里 Init 函数指针真正指向的实现。
 * 2. 对外不直接暴露，所以用 static 修饰。
 */
static int MAX30102_Module_Init_Impl(MAX30102_Module_t *self)
{
    /* 空指针保护，防止野指针调用 */
    if (self == NULL)
    {
        return MAX30102_MODULE_ERROR;
    }

    /* 初始化模块中的数据成员 */
    self->heart_rate = MAX30102_MODULE_INVALID_VALUE;
    self->spo2 = MAX30102_MODULE_INVALID_VALUE;
    self->valid = MAX30102_MODULE_INVALID_FLAG;
    self->max_value = MAX30102_MODULE_MAX_INT;

    /* 调用底层 MAX30102 驱动初始化 */
    max30102_init();

    /* 初始化完成，置位标志 */
    self->is_init = MAX30102_MODULE_VALID_FLAG;
    return MAX30102_MODULE_OK;
}

/*
 * 内部测量函数实现
 *
 * 说明：
 * 1. 调用底层驱动读取心率和血氧。
 * 2. 再把结果保存到模块结构体成员里。
 * 3. 增加基础有效范围判断，减少 0、异常大值、异常小值进入上层算法。
 */
static int MAX30102_Module_Measure_Impl(MAX30102_Module_t *self)
{
    int32_t hr = 0;
    int32_t spo2 = 0;

    /* 空指针检查 */
    if (self == NULL)
    {
        return MAX30102_MODULE_ERROR;
    }

    /* 如果模块还没有初始化，禁止测量 */
    if (self->is_init != MAX30102_MODULE_VALID_FLAG)
    {
        return MAX30102_MODULE_ERROR;
    }

    /* 调用底层驱动读取一组心率、血氧数据 */
    max30102_Read_Data(&hr, &spo2);

    /* 保存到模块结构体中，统一转成 int */
    self->heart_rate = (int)hr;
    self->spo2 = (int)spo2;

    /*
     * 判断本次数据是否有效。
     * 原来只判断 heart_rate > 0 && spo2 > 0，容易把异常数据当成有效。
     * 现在改为基础生理范围判断：
     *   HR   : 40~220 bpm
     *   SpO2 : 70~100 %
     * 更具体的预警等级在 main.c 中判断。
     */
    if ((self->heart_rate >= MAX30102_MODULE_HR_MIN) &&
        (self->heart_rate <= MAX30102_MODULE_HR_MAX) &&
        (self->spo2 >= MAX30102_MODULE_SPO2_MIN) &&
        (self->spo2 <= MAX30102_MODULE_SPO2_MAX))
    {
        self->valid = MAX30102_MODULE_VALID_FLAG;
    }
    else
    {
        self->valid = MAX30102_MODULE_INVALID_FLAG;
    }

    return MAX30102_MODULE_OK;
}

/*
 * 内部取值函数实现
 *
 * 说明：
 * 1. 把模块当前保存的 heart_rate / spo2 / valid 返回给外部。
 * 2. 通过指针参数输出。
 */
static int MAX30102_Module_GetValue_Impl(MAX30102_Module_t *self, int *heart_rate, int *spo2, int *valid)
{
    /* 参数合法性检查 */
    if ((self == NULL) || (heart_rate == NULL) || (spo2 == NULL) || (valid == NULL))
    {
        return MAX30102_MODULE_ERROR;
    }

    /* 把结构体中保存的值输出给外部变量 */
    *heart_rate = self->heart_rate;
    *spo2 = self->spo2;
    *valid = self->valid;

    return MAX30102_MODULE_OK;
}

/*
 * 模块创建函数
 *
 * 作用：
 * 1. 清空整个结构体。
 * 2. 给数据成员赋默认值。
 * 3. 把 3 个内部实现函数绑定到结构体函数指针。
 */
void MAX30102_Module_Create(MAX30102_Module_t *self)
{
    /* 空指针保护 */
    if (self == NULL)
    {
        return;
    }

    /* 整个结构体先清零，保证初始状态干净 */
    memset(self, 0, sizeof(MAX30102_Module_t));

    /* 初始化默认数据 */
    self->heart_rate = MAX30102_MODULE_INVALID_VALUE;
    self->spo2 = MAX30102_MODULE_INVALID_VALUE;
    self->valid = MAX30102_MODULE_INVALID_FLAG;
    self->is_init = MAX30102_MODULE_INVALID_FLAG;
    self->max_value = MAX30102_MODULE_MAX_INT;

    /* 绑定函数指针，外部就可以通过结构体成员调用这 3 个函数 */
    self->Init = MAX30102_Module_Init_Impl;
    self->Measure = MAX30102_Module_Measure_Impl;
    self->GetValue = MAX30102_Module_GetValue_Impl;
}
