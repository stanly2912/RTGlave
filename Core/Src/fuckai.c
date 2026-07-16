#include "rtthread.h"

#include "main.h"
#include "max30102_module.h"

#include <stdio.h>
#include <stdint.h>

/* ========================= 用户基础参数 ========================= */
#define USER_AGE_YEARS                 20
#define USER_WEIGHT_KG                 65
#define USER_SEX_MALE                  1

/* ========================= 心率有效范围 =========================
 * 卡路里只依赖有效心率，不再要求血氧同时有效。
 */
#define HR_VALID_MIN                   40
#define HR_VALID_MAX                   220

/* ========================= 简化后的预警阈值 =========================
 * 心率过低：HR < 50 bpm
 * 心率过高：HR >= 最大心率的 85%，最大心率约为 220-年龄
 * 血氧过低：SpO2 <= 94%
 */
#define HR_LOW_WARN                    50
#define HR_HIGH_PERCENT                85
#define SPO2_LOW_VALUE                 94

/*
 * 每次 MAX30102 测量使用约 5 秒窗口，因此连续 2 个窗口异常后再确认，
 * 可减少单次运动伪影造成的误报。
 */
#define ALERT_CONFIRM_COUNT            1

/* ========================= 预警播报码 =========================
 * 0：正常
 * 1：心率过低
 * 2：心率过高
 * 3：血氧过低
 *
 * 心率和血氧由触点分别选择、分别播报，因此不再生成组合预警码。
 */
#define ALERT_NONE                     0U
#define ALERT_HR_LOW                   1U
#define ALERT_HR_HIGH                  2U
#define ALERT_SPO2_LOW                 3U

#define DATA_OUTPUT_INTERVAL_MS        1000U

static MAX30102_Module_t g_max30102;

/*
 * 唯一的实时健康数据全局变量。
 * 心率、血氧各自拥有独立 valid 和 alert_code。
 */
volatile Health_Data_t g_health_data = {0};

static uint32_t g_total_kcal_x100 = 0;
static uint32_t g_sport_time_ms = 0;
static uint32_t g_last_kcal_tick = 0;

static int g_hr_low_count = 0;
static int g_hr_high_count = 0;
static int g_spo2_low_count = 0;

static uint32_t g_last_data_output_tick = 0;

static int Is_Heart_Rate_Valid_For_Sport(int hr, int hr_valid);
static int Calc_Kcal_Per_Min_X100(int hr);
static void Sport_Calorie_Update(int hr, int hr_valid);
static uint8_t Get_HR_Alert_Code(int hr, int hr_valid);
static uint8_t Get_SPO2_Alert_Code(int spo2, int spo2_valid);
static void Update_Health_Global_Data(int hr,
                                      int spo2,
                                      int hr_valid,
                                      int spo2_valid,
                                      uint8_t hr_alert_code,
                                      uint8_t spo2_alert_code);
static void Output_Data_If_Need(void);

/* 卡路里和运动时间只检查心率，不再被血氧短暂无效卡住。 */
static int Is_Heart_Rate_Valid_For_Sport(int hr, int hr_valid)
{
  if (hr_valid != MAX30102_MODULE_VALID_FLAG)
  {
    return 0;
  }

  if ((hr < HR_VALID_MIN) || (hr > HR_VALID_MAX))
  {
    return 0;
  }

  return 1;
}

static int Calc_Kcal_Per_Min_X100(int hr)
{
  int64_t numerator_x10000;
  int64_t kcal_per_min_x100;

#if USER_SEX_MALE
  numerator_x10000 = -550969LL
                     + 6309LL * hr
                     + 1988LL * USER_WEIGHT_KG
                     + 2017LL * USER_AGE_YEARS;
#else
  numerator_x10000 = -204022LL
                     + 4472LL * hr
                     - 1263LL * USER_WEIGHT_KG
                     + 740LL  * USER_AGE_YEARS;
#endif

  if (numerator_x10000 <= 0)
  {
    return 0;
  }

  kcal_per_min_x100 = (numerator_x10000 * 100LL) / 41840LL;

  if (kcal_per_min_x100 < 0)
  {
    kcal_per_min_x100 = 0;
  }

  if (kcal_per_min_x100 > 100000LL)
  {
    kcal_per_min_x100 = 100000LL;
  }

  return (int)kcal_per_min_x100;
}

static void Sport_Calorie_Update(int hr, int hr_valid)
{
  uint32_t now = HAL_GetTick();
  uint32_t dt_ms;
  int kcal_per_min_x100;
  uint32_t add_kcal_x100;

  if (g_last_kcal_tick == 0U)
  {
    g_last_kcal_tick = now;
    return;
  }

  dt_ms = now - g_last_kcal_tick;
  g_last_kcal_tick = now;

  if (!Is_Heart_Rate_Valid_For_Sport(hr, hr_valid))
  {
    return;
  }

  kcal_per_min_x100 = Calc_Kcal_Per_Min_X100(hr);
  g_sport_time_ms += dt_ms;

  add_kcal_x100 = (uint32_t)(((int64_t)kcal_per_min_x100 * dt_ms + 30000LL) / 60000LL);
  g_total_kcal_x100 += add_kcal_x100;
}

/* 心率功能使用的预警码：0正常、1过低、2过高。 */
static uint8_t Get_HR_Alert_Code(int hr, int hr_valid)
{
  int hr_max;
  int hr_high_value;

  if (hr_valid != MAX30102_MODULE_VALID_FLAG)
  {
    g_hr_low_count = 0;
    g_hr_high_count = 0;
    return ALERT_NONE;
  }

  hr_max = 220 - USER_AGE_YEARS;
  hr_high_value = (hr_max * HR_HIGH_PERCENT) / 100;

  if (hr < HR_LOW_WARN)
  {
    g_hr_low_count++;
  }
  else
  {
    g_hr_low_count = 0;
  }

  if (hr >= hr_high_value)
  {
    g_hr_high_count++;
  }
  else
  {
    g_hr_high_count = 0;
  }

  if (g_hr_high_count >= ALERT_CONFIRM_COUNT)
  {
    return ALERT_HR_HIGH;
  }

  if (g_hr_low_count >= ALERT_CONFIRM_COUNT)
  {
    return ALERT_HR_LOW;
  }

  return ALERT_NONE;
}

/* 血氧功能使用的预警码：0正常、3过低。 */
static uint8_t Get_SPO2_Alert_Code(int spo2, int spo2_valid)
{
  if (spo2_valid != MAX30102_MODULE_VALID_FLAG)
  {
    g_spo2_low_count = 0;
    return ALERT_NONE;
  }

  if (spo2 <= SPO2_LOW_VALUE)
  {
    g_spo2_low_count++;
  }
  else
  {
    g_spo2_low_count = 0;
  }

  if (g_spo2_low_count >= ALERT_CONFIRM_COUNT)
  {
    return ALERT_SPO2_LOW;
  }

  return ALERT_NONE;
}

static void Update_Health_Global_Data(int hr,
                                      int spo2,
                                      int hr_valid,
                                      int spo2_valid,
                                      uint8_t hr_alert_code,
                                      uint8_t spo2_alert_code)
{
  g_health_data.heart_rate = hr;
  g_health_data.spo2 = spo2;
  g_health_data.hr_valid = hr_valid;
  g_health_data.spo2_valid = spo2_valid;

  /* 兼容字段：任意一项有效时 valid=1。具体功能应读取各自 valid。 */
  g_health_data.valid = ((hr_valid == MAX30102_MODULE_VALID_FLAG) ||
                         (spo2_valid == MAX30102_MODULE_VALID_FLAG)) ? 1 : 0;

  g_health_data.kcal_x100 = g_total_kcal_x100;
  g_health_data.sport_time_s = g_sport_time_ms / 1000U;
  g_health_data.hr_alert_code = hr_alert_code;
  g_health_data.spo2_alert_code = spo2_alert_code;
}

static void Output_Data_If_Need(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - g_last_data_output_tick) < DATA_OUTPUT_INTERVAL_MS)
  {
    return;
  }
  g_last_data_output_tick = now;

  /*
   * 调试输出格式：
   * HR/HRV：心率及其有效标志
   * SPO2/SPO2V：血氧及其有效标志
   * TIME/KCAL：运动时间与累计卡路里
   * HRA：心率预警码，0正常、1过低、2过高
   * SPO2A：血氧预警码，0正常、3过低
   *
   * 需要串口调试时可取消下面 printf 的注释。
   */
  /*
  printf("DATA:HR=%d,HRV=%d,SPO2=%d,SPO2V=%d,TIME=%lu,KCAL=%lu.%02lu,HRA=%u,SPO2A=%u\r\n",
         g_health_data.heart_rate,
         g_health_data.hr_valid,
         g_health_data.spo2,
         g_health_data.spo2_valid,
         (unsigned long)g_health_data.sport_time_s,
         (unsigned long)(g_health_data.kcal_x100 / 100U),
         (unsigned long)(g_health_data.kcal_x100 % 100U),
         (unsigned int)g_health_data.hr_alert_code,
         (unsigned int)g_health_data.spo2_alert_code);
  */
}

void app4(void *param)
{
  int hr = 0;
  int spo2 = 0;
  int hr_valid = 0;
  int spo2_valid = 0;
  uint8_t hr_alert_code = ALERT_NONE;
  uint8_t spo2_alert_code = ALERT_NONE;

  (void)param;

  MAX30102_Module_Create(&g_max30102);

  (void)g_max30102.Init(&g_max30102);

  while (1)
  {
    if (g_max30102.Measure(&g_max30102) == MAX30102_MODULE_OK)
    {
      g_max30102.GetValue(&g_max30102,
                          &hr,
                          &spo2,
                          &hr_valid,
                          &spo2_valid);

      /* 卡路里与运动时间仅依赖有效心率。 */
      Sport_Calorie_Update(hr, hr_valid);

      /* 心率和血氧分别产生各自的播报码。 */
      hr_alert_code = Get_HR_Alert_Code(hr, hr_valid);
      spo2_alert_code = Get_SPO2_Alert_Code(spo2, spo2_valid);

      Update_Health_Global_Data(hr,
                                spo2,
                                hr_valid,
                                spo2_valid,
                                hr_alert_code,
                                spo2_alert_code);

      Output_Data_If_Need();
    }

    rt_thread_mdelay(500);
  }
}
