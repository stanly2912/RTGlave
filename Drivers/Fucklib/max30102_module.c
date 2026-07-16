#include "max30102_module.h"
#include <string.h>

/* 内部初始化函数实现 */
static int MAX30102_Module_Init_Impl(MAX30102_Module_t *self)
{
    if (self == NULL)
    {
        return MAX30102_MODULE_ERROR;
    }

    self->heart_rate = MAX30102_MODULE_INVALID_VALUE;
    self->spo2 = MAX30102_MODULE_INVALID_VALUE;
    self->hr_valid = MAX30102_MODULE_INVALID_FLAG;
    self->spo2_valid = MAX30102_MODULE_INVALID_FLAG;
    self->valid = MAX30102_MODULE_INVALID_FLAG;
    self->max_value = MAX30102_MODULE_MAX_INT;

    max30102_init();

    self->is_init = MAX30102_MODULE_VALID_FLAG;
    return MAX30102_MODULE_OK;
}

/*
 * 内部测量函数实现。
 * 心率和血氧分别保存、分别判断有效性，某一项失效不会把另一项一起清零。
 */
static int MAX30102_Module_Measure_Impl(MAX30102_Module_t *self)
{
    int32_t hr = 0;
    int32_t spo2 = 0;
    int hr_valid = 0;
    int spo2_valid = 0;

    if (self == NULL)
    {
        return MAX30102_MODULE_ERROR;
    }

    if (self->is_init != MAX30102_MODULE_VALID_FLAG)
    {
        return MAX30102_MODULE_ERROR;
    }

    max30102_Read_Data(&hr, &spo2, &hr_valid, &spo2_valid);

    self->heart_rate = (int)hr;
    self->spo2 = (int)spo2;
    self->hr_valid = hr_valid;
    self->spo2_valid = spo2_valid;

    /*
     * valid 仅为兼容旧代码：心率或血氧任意一项有效时置 1。
     * 新代码应按功能分别读取 hr_valid / spo2_valid。
     */
    self->valid = ((hr_valid == MAX30102_MODULE_VALID_FLAG) ||
                   (spo2_valid == MAX30102_MODULE_VALID_FLAG))
                      ? MAX30102_MODULE_VALID_FLAG
                      : MAX30102_MODULE_INVALID_FLAG;

    return MAX30102_MODULE_OK;
}

/* 返回当前心率、血氧及各自有效标志。 */
static int MAX30102_Module_GetValue_Impl(MAX30102_Module_t *self,
                                          int *heart_rate,
                                          int *spo2,
                                          int *hr_valid,
                                          int *spo2_valid)
{
    if ((self == NULL) || (heart_rate == NULL) || (spo2 == NULL) ||
        (hr_valid == NULL) || (spo2_valid == NULL))
    {
        return MAX30102_MODULE_ERROR;
    }

    *heart_rate = self->heart_rate;
    *spo2 = self->spo2;
    *hr_valid = self->hr_valid;
    *spo2_valid = self->spo2_valid;

    return MAX30102_MODULE_OK;
}

void MAX30102_Module_Create(MAX30102_Module_t *self)
{
    if (self == NULL)
    {
        return;
    }

    memset(self, 0, sizeof(MAX30102_Module_t));

    self->heart_rate = MAX30102_MODULE_INVALID_VALUE;
    self->spo2 = MAX30102_MODULE_INVALID_VALUE;
    self->hr_valid = MAX30102_MODULE_INVALID_FLAG;
    self->spo2_valid = MAX30102_MODULE_INVALID_FLAG;
    self->valid = MAX30102_MODULE_INVALID_FLAG;
    self->is_init = MAX30102_MODULE_INVALID_FLAG;
    self->max_value = MAX30102_MODULE_MAX_INT;

    self->Init = MAX30102_Module_Init_Impl;
    self->Measure = MAX30102_Module_Measure_Impl;
    self->GetValue = MAX30102_Module_GetValue_Impl;
}
