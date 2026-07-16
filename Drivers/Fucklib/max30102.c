#include "max30102.h"
#include "main.h"
#include "rtthread.h"

#define MAX_BRIGHTNESS 255

extern I2C_HandleTypeDef hi2c1;

/*
 * 算法缓存：125 点。
 * MAX30102 硬件按 100Hz 采样，每 4 点平均为 1 点，
 * 因此 125 点仍代表约 5 秒数据，同时 RAM 占用较小。
 */
uint32_t aun_ir_buffer[BUFFER_SIZE];
int32_t n_ir_buffer_length;
uint32_t aun_red_buffer[BUFFER_SIZE];
int32_t n_sp02;
int8_t ch_spo2_valid;
int32_t n_heart_rate;
int8_t ch_hr_valid;
uint8_t uch_dummy;

static void max30102_read_one_sample(uint32_t *red, uint32_t *ir)
{
    uint8_t temp[6] = {0};

    max30102_FIFO_ReadBytes(REG_FIFO_DATA, temp);

    *red = ((uint32_t)(temp[0] & 0x03U) << 16) |
           ((uint32_t)temp[1] << 8) |
           ((uint32_t)temp[2]);

    *ir  = ((uint32_t)(temp[3] & 0x03U) << 16) |
           ((uint32_t)temp[4] << 8) |
           ((uint32_t)temp[5]);
}

/*
 * 采集一整个算法窗口。
 * 传感器原始采样率为 100Hz，每 4 个原始样本取平均，
 * 得到 25Hz 的算法输入；125 点对应约 5 秒。
 */
static void max30102_collect_algorithm_window(void)
{
    int i;
    int j;

    n_ir_buffer_length = BUFFER_SIZE;

    for (i = 0; i < n_ir_buffer_length; i++)
    {
        uint32_t red_sum = 0U;
        uint32_t ir_sum = 0U;

        for (j = 0; j < MAX30102_DOWNSAMPLE_FACTOR; j++)
        {
            uint32_t red_raw = 0U;
            uint32_t ir_raw = 0U;

            /* 使用毫秒延时，避免 rt_thread_delay 的 tick 单位造成采样率错误。 */
            rt_thread_mdelay(1000U / MAX30102_SENSOR_FS);
            max30102_read_one_sample(&red_raw, &ir_raw);

            red_sum += red_raw;
            ir_sum += ir_raw;
        }

        aun_red_buffer[i] = red_sum / MAX30102_DOWNSAMPLE_FACTOR;
        aun_ir_buffer[i] = ir_sum / MAX30102_DOWNSAMPLE_FACTOR;
    }
}

uint8_t max30102_Bus_Write(uint8_t Register_Address, uint8_t Word_Data)
{
    if (HAL_I2C_Mem_Write(&hi2c1,
                          MAX30102_I2C_ADDR,
                          Register_Address,
                          I2C_MEMADD_SIZE_8BIT,
                          &Word_Data,
                          1,
                          100) == HAL_OK)
    {
        return 1;
    }
    return 0;
}

uint8_t max30102_Bus_Read(uint8_t Register_Address)
{
    uint8_t data = 0;

    if (HAL_I2C_Mem_Read(&hi2c1,
                         MAX30102_I2C_ADDR,
                         Register_Address,
                         I2C_MEMADD_SIZE_8BIT,
                         &data,
                         1,
                         100) == HAL_OK)
    {
        return data;
    }
    return 0;
}

void max30102_FIFO_ReadWords(uint8_t Register_Address, uint16_t Word_Data[][2], uint8_t count)
{
    uint8_t buf[4 * 32] = {0};
    uint8_t i;

    if (count == 0 || count > 32)
    {
        return;
    }

    if (HAL_I2C_Mem_Read(&hi2c1,
                         MAX30102_I2C_ADDR,
                         Register_Address,
                         I2C_MEMADD_SIZE_8BIT,
                         buf,
                         (uint16_t)(count * 4U),
                         100) != HAL_OK)
    {
        return;
    }

    for (i = 0; i < count; i++)
    {
        Word_Data[i][0] = ((uint16_t)buf[4 * i] << 8) | buf[4 * i + 1];
        Word_Data[i][1] = ((uint16_t)buf[4 * i + 2] << 8) | buf[4 * i + 3];
    }
}

void max30102_FIFO_ReadBytes(uint8_t Register_Address, uint8_t *Data)
{
    uint8_t dummy = 0;

    (void)HAL_I2C_Mem_Read(&hi2c1,
                           MAX30102_I2C_ADDR,
                           REG_INTR_STATUS_1,
                           I2C_MEMADD_SIZE_8BIT,
                           &dummy,
                           1,
                           100);
    (void)HAL_I2C_Mem_Read(&hi2c1,
                           MAX30102_I2C_ADDR,
                           REG_INTR_STATUS_2,
                           I2C_MEMADD_SIZE_8BIT,
                           &dummy,
                           1,
                           100);

    (void)HAL_I2C_Mem_Read(&hi2c1,
                           MAX30102_I2C_ADDR,
                           Register_Address,
                           I2C_MEMADD_SIZE_8BIT,
                           Data,
                           6,
                           100);
}

void max30102_init(void)
{
    rt_thread_mdelay(10);

    max30102_reset();
    rt_thread_mdelay(20);

    max30102_Bus_Write(REG_INTR_ENABLE_1, 0xC0);
    max30102_Bus_Write(REG_INTR_ENABLE_2, 0x00);
    max30102_Bus_Write(REG_FIFO_WR_PTR,  0x00);
    max30102_Bus_Write(REG_OVF_COUNTER,  0x00);
    max30102_Bus_Write(REG_FIFO_RD_PTR,  0x00);
    max30102_Bus_Write(REG_FIFO_CONFIG,  0x0F);
    max30102_Bus_Write(REG_MODE_CONFIG,  0x03);
    max30102_Bus_Write(REG_SPO2_CONFIG,  0x27);
    max30102_Bus_Write(REG_LED1_PA,      0x08);
    max30102_Bus_Write(REG_LED2_PA,      0x08);
    max30102_Bus_Write(REG_PILOT_PA,     0x08);

    rt_thread_mdelay(100);

    max30102_collect_algorithm_window();

    maxim_heart_rate_and_oxygen_saturation(
        aun_ir_buffer,
        n_ir_buffer_length,
        aun_red_buffer,
        &n_sp02,
        &ch_spo2_valid,
        &n_heart_rate,
        &ch_hr_valid);
}

void max30102_reset(void)
{
    max30102_Bus_Write(REG_MODE_CONFIG, 0x40);
    rt_thread_mdelay(20);
}

void maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data)
{
    (void)max30102_Bus_Write(uch_addr, uch_data);
}

void maxim_max30102_read_reg(uint8_t uch_addr, uint8_t *puch_data)
{
    *puch_data = max30102_Bus_Read(uch_addr);
}

void maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
    uint32_t un_temp;
    uint8_t ach_i2c_data[6] = {0};

    *pun_red_led = 0;
    *pun_ir_led = 0;

    max30102_FIFO_ReadBytes(REG_FIFO_DATA, ach_i2c_data);

    un_temp = (uint32_t)ach_i2c_data[0];
    un_temp <<= 16;
    *pun_red_led += un_temp;
    un_temp = (uint32_t)ach_i2c_data[1];
    un_temp <<= 8;
    *pun_red_led += un_temp;
    un_temp = (uint32_t)ach_i2c_data[2];
    *pun_red_led += un_temp;

    un_temp = (uint32_t)ach_i2c_data[3];
    un_temp <<= 16;
    *pun_ir_led += un_temp;
    un_temp = (uint32_t)ach_i2c_data[4];
    un_temp <<= 8;
    *pun_ir_led += un_temp;
    un_temp = (uint32_t)ach_i2c_data[5];
    *pun_ir_led += un_temp;

    *pun_red_led &= 0x03FFFFU;
    *pun_ir_led  &= 0x03FFFFU;
}

void max30102_Read_Data(int32_t *heart_rate,
                         int32_t *spo2,
                         int *hr_valid,
                         int *spo2_valid)
{
    if ((heart_rate == NULL) || (spo2 == NULL) ||
        (hr_valid == NULL) || (spo2_valid == NULL))
    {
        return;
    }

    *heart_rate = 0;
    *spo2 = 0;
    *hr_valid = 0;
    *spo2_valid = 0;

    max30102_collect_algorithm_window();

    maxim_heart_rate_and_oxygen_saturation(
        aun_ir_buffer,
        n_ir_buffer_length,
        aun_red_buffer,
        &n_sp02,
        &ch_spo2_valid,
        &n_heart_rate,
        &ch_hr_valid);

    /* 心率和血氧分别判断有效性，互不连带清零。 */
    if ((ch_hr_valid == 1) &&
        (n_heart_rate >= 40) && (n_heart_rate <= 220))
    {
        *heart_rate = n_heart_rate;
        *hr_valid = 1;
    }

    if (ch_spo2_valid == 1)
    {
        /* 有效算法结果若偶尔超过 100%，显示值按 100% 处理，不做过高预警。 */
        if (n_sp02 > 100)
        {
            n_sp02 = 100;
        }

        if (n_sp02 >= 70)
        {
            *spo2 = n_sp02;
            *spo2_valid = 1;
        }
    }
}
