#include "max30102.h"
#include "main.h"
#include "rtthread.h"

#define MAX_BRIGHTNESS 255

extern I2C_HandleTypeDef hi2c1;

uint32_t aun_ir_buffer[BUFFER_SIZE];   /* IR 数据缓存：100 点，节省 RAM */
int32_t n_ir_buffer_length;
uint32_t aun_red_buffer[BUFFER_SIZE];  /* RED 数据缓存：100 点，节省 RAM */
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
    int i;

    rt_thread_delay(10);

    max30102_reset();
    rt_thread_delay(20);

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

    rt_thread_delay(100);

    n_ir_buffer_length = BUFFER_SIZE;

    for (i = 0; i < n_ir_buffer_length; i++)
    {
        rt_thread_delay(10);
        max30102_read_one_sample(&aun_red_buffer[i], &aun_ir_buffer[i]);
    }

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
    rt_thread_delay(20);
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

void max30102_Read_Data(int32_t *heart_rate, int32_t *sp02)
{
    int i;

    /*
     * RAM 优化说明：
     * 原始例程使用 500 点 RED/IR 缓存，并且 algorithm.c 中还有多个 500 点数组，
     * 在 STM32F103C8T6（20KB RAM）上很容易爆 RAM。
     *
     * 这里统一使用 algorithm.h 中的 BUFFER_SIZE，目前为 100 点。
     * 每次读取 100 个样本后计算一次心率和血氧，RAM 占用会明显降低。
     */
    n_ir_buffer_length = BUFFER_SIZE;

    for (i = 0; i < n_ir_buffer_length; i++)
    {
        rt_thread_delay(10);
        max30102_read_one_sample(&aun_red_buffer[i], &aun_ir_buffer[i]);
    }

    maxim_heart_rate_and_oxygen_saturation(
        aun_ir_buffer,
        n_ir_buffer_length,
        aun_red_buffer,
        &n_sp02,
        &ch_spo2_valid,
        &n_heart_rate,
        &ch_hr_valid);

    /*
     * 这里只做底层数据合理性判断：
     * 心率：40~220 bpm
     * 血氧：70~100 %
     * 更具体的预警逻辑放在 main.c 的 g_health_data.alert_code 中处理。
     */
    if ((ch_hr_valid == 1) && (ch_spo2_valid == 1) &&
        (n_heart_rate >= 40) && (n_heart_rate <= 220) &&
        (n_sp02 >= 70) && (n_sp02 <= 100))
    {
        *heart_rate = n_heart_rate;
        *sp02 = n_sp02;
    }
    else
    {
        *heart_rate = 0;
        *sp02 = 0;
    }
}








