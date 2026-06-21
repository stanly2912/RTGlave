#include "rtthread.h"

#include "main.h"
#include "max30102_module.h"

#include <stdio.h>
#include <stdint.h>

/*
 * 实时健康数据全局变量结构体。
 *
 * 只保留一个全局变量 g_health_data。
 * MAX30102 模块测到的数据、卡路里、运动时间、预警标志都会写到这里。
 * 后续蓝牙/App/语音模块需要数据时，直接读取 g_health_data 即可。
 *
 * alert_code 是“哪里出问题”的预警播报码，不再做很多等级。
 * 语音模块不用再做按位与判断，直接根据这个数值选择播报内容即可：
 * 0：无异常，不播异常提醒
 * 1：心率过低，建议播“注意，心率过低，请检查身体状态”
 * 2：心率过高，建议播“注意，心率过高，请降低骑行强度”
 * 4：血氧偏低，建议播“注意，血氧偏低，请降低骑行强度并休息”
 * 5：心率过低 + 血氧偏低，建议播“注意，心率过低且血氧偏低，请检查身体状态并休息”
 * 6：心率过高 + 血氧偏低，建议播“注意，心率过高且血氧偏低，请停止骑行并休息”
 *
 * normal_report_flag 用于正常骑行时的周期播报：
 * 0：没有到正常播报时间
 * 1：已到正常播报时间，语音模块可以播报一次正常数据，播完后由语音模块清 0
 * 正常播这个：已骑行 X 分钟，消耗约 Y 千卡，当前心率 Z，血氧 W
 * normal_report_count 表示已经触发过几次正常播报，方便调试。
 */
typedef struct
{
  volatile int heart_rate;                /* 当前心率，单位 bpm */
  volatile int spo2;                      /* 当前血氧，单位 % */
  volatile int valid;                     /* MAX30102 数据有效标志，1 表示有效 */
  volatile uint32_t kcal_x100;            /* 累计卡路里*100，例如 18.36 kcal 存 1836 */
  volatile uint32_t sport_time_s;         /* 有效运动时间，单位秒 */
  volatile uint8_t alert_code;           /* 预警播报码，表示心率/血氧哪里异常 */
  volatile uint8_t normal_report_flag;    /* 正常数据播报标志，1 表示到 3 分钟播报点 */
  volatile uint32_t normal_report_count;  /* 正常数据播报触发次数 */
} Health_Data_t;

/* ========================= 用户基础参数 =========================
 * 卡路里估算需要年龄、体重、性别。
 * 现在先写死，后期可以改成蓝牙/App/按键输入。
 */
#define USER_AGE_YEARS                 20      /* 年龄，单位：岁 */
#define USER_WEIGHT_KG                 65      /* 体重，单位：kg */
#define USER_SEX_MALE                  1       /* 1 表示男，0 表示女 */

/* ========================= 心率/血氧有效范围 =========================
 * 用于过滤明显不可信的数据。
 * 只有 valid=1 且 HR/SpO2 在合理范围内，才累计运动时间和卡路里。
 */
#define HR_VALID_MIN                   40
#define HR_VALID_MAX                   220
#define SPO2_VALID_MIN                 70
#define SPO2_VALID_MAX                 100

/* ========================= 简化后的预警阈值 =========================
 * 语音播报不再分“预警/危险/严重”等很多级别，只判断哪里异常：
 * 心率过低：HR < 50
 * 心率过高：HR >= 最大心率的 85%，最大心率 = 220 - 年龄
 * 血氧偏低：SpO2 <= 94
 */
#define HR_LOW_WARN                    50
#define HR_HIGH_PERCENT                85
#define SPO2_LOW_VALUE                 94

/* ========================= 连续异常确认次数 =========================
 * 当前 main 循环末尾 HAL_Delay(500)，理论上 2 次约 1 秒，6 次约 3 秒。
 * 连续 6 次异常才置位 alert_code，可以减少 MAX30102 因接触不稳/运动抖动产生的误报。
 */
#define ALERT_CONFIRM_COUNT            6

/* ========================= 预警播报码 alert_code =========================
 * 语音模块直接读取 g_health_data.alert_code，按具体数值选择播报内容。
 *
 * 0：无异常
 * 1：心率过低
 * 2：心率过高
 * 4：血氧偏低
 * 5：心率过低 + 血氧偏低
 * 6：心率过高 + 血氧偏低
 *
 * 说明：这里故意不用 3，因为“心率过低”和“心率过高”不会同时成立。
 */
#define ALERT_NONE                     0U
#define ALERT_HR_LOW                   1U
#define ALERT_HR_HIGH                  2U
#define ALERT_SPO2_LOW                 4U
#define ALERT_HR_LOW_SPO2_LOW          5U
#define ALERT_HR_HIGH_SPO2_LOW         6U

/* ========================= 正常数据播报节奏 =========================
 * 正常骑行时，每累计 3 分钟有效运动时间，置位一次 normal_report_flag。
 * 语音模块读到 normal_report_flag=1 后，可以播报当前运动数据，播完后清 0。
 */
#define NORMAL_REPORT_INTERVAL_MS      180000U     /* 3 分钟 = 180000 ms */

/* ========================= 串口调试输出节奏 =========================
 * 普通 DATA 行：继续用串口 printf 输出，用于调试/上位机显示，默认 1 秒输出一次。
 * 这里只负责调试输出；真正给其他模块读取的数据都放在 g_health_data 全局变量里。
 */
#define DATA_OUTPUT_INTERVAL_MS        1000U       /* 普通数据输出间隔：1 秒 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static MAX30102_Module_t g_max30102;

/*
 * 实时健康数据全局变量。
 * 后续蓝牙/App/语音模块统一读取这个变量，不再额外维护语音播报全局变量。
 *
 * g_health_data.heart_rate：当前心率，单位 bpm
 * g_health_data.spo2：当前血氧，单位 %
 * g_health_data.valid：数据有效标志，1 表示有效，0 表示无效
 * g_health_data.kcal_x100：累计卡路里*100，例如 18.36 kcal 存 1836
 * g_health_data.sport_time_s：有效运动时间，单位秒
 * g_health_data.alert_code：预警播报码，0无异常，1心率过低，2心率过高，4血氧偏低，5心率过低+血氧偏低，6心率过高+血氧偏低
 * g_health_data.normal_report_flag：正常数据播报标志，1表示到3分钟播报点，语音模块播完后清0
 * g_health_data.normal_report_count：正常数据播报触发次数，主要用于调试
 */
volatile Health_Data_t g_health_data = {0};

/* 卡路里累计值，单位是 0.01 kcal。
 * 例如 g_total_kcal_x100 = 1234，表示 12.34 kcal。
 * 这样做可以避免 printf 浮点数带来的工程配置问题。
 */
static uint32_t g_total_kcal_x100 = 0;

/* 有效运动时间，单位 ms。只有数据有效时才累计。 */
static uint32_t g_sport_time_ms = 0;

/* 上一次更新卡路里的系统时间，单位 ms，来自 HAL_GetTick()。 */
static uint32_t g_last_kcal_tick = 0;

/* 上一次触发正常数据播报时的有效运动时间，单位 ms。 */
static uint32_t g_last_normal_report_sport_ms = 0;

/* 连续异常计数，用于减少误报。 */
static int g_hr_low_count = 0;
static int g_hr_high_count = 0;
static int g_spo2_low_count = 0;

/* 普通数据输出计时，避免串口刷屏。 */
static uint32_t g_last_data_output_tick = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static int Is_Data_Valid_For_Sport(int hr, int spo2, int valid);
static int Calc_Kcal_Per_Min_X100(int hr);
static void Sport_Calorie_Update(int hr, int spo2, int valid);
static uint8_t Get_Alert_Code(int hr, int spo2, int valid);
static void Update_Health_Global_Data(int hr, int spo2, int valid, uint8_t alert_code);
static void Update_Normal_Report_Flag(int hr, int spo2, int valid, uint8_t alert_code);
static void Output_Data_If_Need(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief 判断当前数据是否可以用于运动时间和卡路里累计
  * @note  valid 由 MAX30102 模块给出，这里再额外检查 HR/SpO2 合理范围。
  */
static int Is_Data_Valid_For_Sport(int hr, int spo2, int valid)
{
  if (valid != MAX30102_MODULE_VALID_FLAG)
  {
    return 0;
  }

  if ((hr < HR_VALID_MIN) || (hr > HR_VALID_MAX))
  {
    return 0;
  }

  if ((spo2 < SPO2_VALID_MIN) || (spo2 > SPO2_VALID_MAX))
  {
    return 0;
  }

  return 1;
}

/**
  * @brief 根据心率估算每分钟消耗的卡路里
  * @param hr 当前心率，单位 bpm
  * @retval 每分钟卡路里消耗，单位 0.01 kcal/min
  *
  * @note 这里使用基于心率、体重、年龄、性别的估算公式。
  *       为了避免嵌入式 printf 浮点支持问题，内部用定点数计算。
  *       返回值为 kcal/min * 100。
  *
  * 男：kcal/min = (-55.0969 + 0.6309*HR + 0.1988*W + 0.2017*A) / 4.184
  * 女：kcal/min = (-20.4022 + 0.4472*HR - 0.1263*W + 0.0740*A) / 4.184
  */
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

  /* 除以 4.184，同时把结果放大 100 倍。
   * numerator_x10000 / 10000 / 4.184 * 100
   * 等价于 numerator_x10000 * 100 / 41840
   */
  kcal_per_min_x100 = (numerator_x10000 * 100LL) / 41840LL;

  if (kcal_per_min_x100 < 0)
  {
    kcal_per_min_x100 = 0;
  }

  if (kcal_per_min_x100 > 100000LL)
  {
    kcal_per_min_x100 = 100000LL;  /* 防止异常数据导致数值过大 */
  }

  return (int)kcal_per_min_x100;
}

/**
  * @brief 更新有效运动时间和累计卡路里
  * @note  不需要新开硬件定时器，直接使用 HAL_GetTick() 计算两次循环间隔。
  */
static void Sport_Calorie_Update(int hr, int spo2, int valid)
{
  uint32_t now = HAL_GetTick();
  uint32_t dt_ms;
  int kcal_per_min_x100;
  uint32_t add_kcal_x100;

  if (g_last_kcal_tick == 0)
  {
    g_last_kcal_tick = now;
    return;
  }

  dt_ms = now - g_last_kcal_tick;
  g_last_kcal_tick = now;

  /* 数据无效时不累计运动时间，也不累计卡路里 */
  if (!Is_Data_Valid_For_Sport(hr, spo2, valid))
  {
    return;
  }

  kcal_per_min_x100 = Calc_Kcal_Per_Min_X100(hr);

  /* 累计有效运动时间 */
  g_sport_time_ms += dt_ms;

  /* kcal/min 转换为本次 dt_ms 对应的 kcal。
   * g_total_kcal_x100 单位是 0.01 kcal。
   */
  add_kcal_x100 = (uint32_t)(((int64_t)kcal_per_min_x100 * dt_ms + 30000LL) / 60000LL);
  g_total_kcal_x100 += add_kcal_x100;
}

/**
  * @brief 获取简化后的预警播报码
  * @note  不再输出很多预警等级，只判断“哪里异常”。
  *        语音模块可以直接 switch(g_health_data.alert_code) 选择播报内容。
  *
  *        alert_code 含义：
  *        0：无异常
  *        1：心率过低
  *        2：心率过高
  *        4：血氧偏低
  *        5：心率过低 + 血氧偏低
  *        6：心率过高 + 血氧偏低
  *
  *        为了减少误报，需要连续 ALERT_CONFIRM_COUNT 次异常才返回对应播报码。
  */
static uint8_t Get_Alert_Code(int hr, int spo2, int valid)
{
  int hr_max;
  int hr_high_value;
  uint8_t hr_low_confirmed = 0U;
  uint8_t hr_high_confirmed = 0U;
  uint8_t spo2_low_confirmed = 0U;

  if (!Is_Data_Valid_For_Sport(hr, spo2, valid))
  {
    g_hr_low_count = 0;
    g_hr_high_count = 0;
    g_spo2_low_count = 0;
    return ALERT_NONE;
  }

  hr_max = 220 - USER_AGE_YEARS;
  hr_high_value = (hr_max * HR_HIGH_PERCENT) / 100;

  /* 心率过低判断 */
  if (hr < HR_LOW_WARN)
  {
    g_hr_low_count++;
  }
  else
  {
    g_hr_low_count = 0;
  }

  /* 心率过高判断 */
  if (hr >= hr_high_value)
  {
    g_hr_high_count++;
  }
  else
  {
    g_hr_high_count = 0;
  }

  /* 血氧偏低判断。血氧一般不做“过高”报警，>100 已在有效性判断中过滤。 */
  if (spo2 <= SPO2_LOW_VALUE)
  {
    g_spo2_low_count++;
  }
  else
  {
    g_spo2_low_count = 0;
  }

  if (g_hr_low_count >= ALERT_CONFIRM_COUNT)
  {
    hr_low_confirmed = 1U;
  }

  if (g_hr_high_count >= ALERT_CONFIRM_COUNT)
  {
    hr_high_confirmed = 1U;
  }

  if (g_spo2_low_count >= ALERT_CONFIRM_COUNT)
  {
    spo2_low_confirmed = 1U;
  }

  /* 组合异常优先返回具体播报码，方便语音模块直接选择语音。 */
  if ((hr_high_confirmed == 1U) && (spo2_low_confirmed == 1U))
  {
    return ALERT_HR_HIGH_SPO2_LOW;       /* 6：心率过高 + 血氧偏低 */
  }

  if ((hr_low_confirmed == 1U) && (spo2_low_confirmed == 1U))
  {
    return ALERT_HR_LOW_SPO2_LOW;        /* 5：心率过低 + 血氧偏低 */
  }

  if (hr_high_confirmed == 1U)
  {
    return ALERT_HR_HIGH;                /* 2：心率过高 */
  }

  if (hr_low_confirmed == 1U)
  {
    return ALERT_HR_LOW;                 /* 1：心率过低 */
  }

  if (spo2_low_confirmed == 1U)
  {
    return ALERT_SPO2_LOW;               /* 4：血氧偏低 */
  }

  return ALERT_NONE;                     /* 0：无异常 */
}

/**
  * @brief 更新实时健康数据全局变量
  * @note  现在只保留 g_health_data 一个全局变量，给蓝牙/App/语音模块读取。
  */
static void Update_Health_Global_Data(int hr, int spo2, int valid, uint8_t alert_code)
{
  g_health_data.heart_rate = hr;
  g_health_data.spo2 = spo2;
  g_health_data.valid = valid;
  g_health_data.kcal_x100 = g_total_kcal_x100;
  g_health_data.sport_time_s = g_sport_time_ms / 1000U;
  g_health_data.alert_code = alert_code;
}

/**
  * @brief 正常骑行每 3 分钟触发一次正常数据播报标志
  * @note  这里只把 normal_report_flag 置 1，不直接播报。
  *        语音模块读到 normal_report_flag=1 后，可以播报：
  *        “已骑行 X 分钟，消耗约 Y 千卡，当前心率 Z，血氧 W”。
  *        播报完成后，语音模块需要执行：g_health_data.normal_report_flag = 0;
  */
static void Update_Normal_Report_Flag(int hr, int spo2, int valid, uint8_t alert_code)
{
  if (!Is_Data_Valid_For_Sport(hr, spo2, valid))
  {
    return;
  }

  /* 有异常时优先播异常，不触发正常数据播报 */
  if (alert_code != ALERT_NONE)
  {
    return;
  }

  if ((g_sport_time_ms - g_last_normal_report_sport_ms) >= NORMAL_REPORT_INTERVAL_MS)
  {
    /* 用加法而不是直接等于当前时间，可以减少循环延迟导致的累计误差 */
    g_last_normal_report_sport_ms += NORMAL_REPORT_INTERVAL_MS;

    g_health_data.normal_report_flag = 1U;
    g_health_data.normal_report_count++;
  }
}

/**
  * @brief 普通数据输出：保留串口调试输出
  * @note  DATA 行只用于串口助手/上位机调试。
  *        其他模块需要实时数据时，直接读取 g_health_data。
  */
static void Output_Data_If_Need(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - g_last_data_output_tick) < DATA_OUTPUT_INTERVAL_MS)
  {
    return;
  }
  g_last_data_output_tick = now;

  /*
   * 调试输出说明：
   * HR：心率，单位 bpm
   * SPO2：血氧，单位 %
   * VALID：数据有效标志，1有效，0无效
   * TIME：有效运动时间，单位秒
   * KCAL：累计卡路里，显示到 0.01 kcal
   * ALERT：预警播报码，0正常，1心率过低，2心率过高，4血氧偏低，5心率过低+血氧偏低，6心率过高+血氧偏低
   * REPORT：正常数据播报标志，1表示到3分钟播报点，语音模块播完后清0
   */

   /*
  printf("DATA:HR=%d,SPO2=%d,VALID=%d,TIME=%lu,KCAL=%lu.%02lu,ALERT=%u,REPORT=%u\r\n",
         g_health_data.heart_rate,
         g_health_data.spo2,
         g_health_data.valid,
         (unsigned long)g_health_data.sport_time_s,
         (unsigned long)(g_health_data.kcal_x100 / 100U),
         (unsigned long)(g_health_data.kcal_x100 % 100U),
         (unsigned int)g_health_data.alert_code,
         (unsigned int)g_health_data.normal_report_flag);*/
}


void app4(void *param) {
  int hr = 0;
  int spo2 = 0;
  int valid = 0;
  uint8_t alert_code = ALERT_NONE;

  MAX30102_Module_Create(&g_max30102);

//   printf("boot\r\n");
//   printf("user age=%d,weight=%d,sex=%d\r\n", USER_AGE_YEARS, USER_WEIGHT_KG, USER_SEX_MALE);

  if (g_max30102.Init(&g_max30102) != MAX30102_MODULE_OK)
  {
    // printf("max30102 init error\r\n");
  }
  else
  {
    // printf("max30102 init ok\r\n");
  }

  while (1)
  {
    if (g_max30102.Measure(&g_max30102) == MAX30102_MODULE_OK)
    {
      g_max30102.GetValue(&g_max30102, &hr, &spo2, &valid);

      /* 1. 根据当前 HR/SpO2 更新运动时间和卡路里 */
      Sport_Calorie_Update(hr, spo2, valid);

      /* 2. 判断“哪里异常”，得到具体预警播报码 */
      alert_code = Get_Alert_Code(hr, spo2, valid);

      /* 3. 更新实时健康数据全局变量，给蓝牙/App/语音模块读取 */
      Update_Health_Global_Data(hr, spo2, valid, alert_code);

      /* 4. 正常骑行每 3 分钟置位一次 normal_report_flag，供语音模块播报正常数据 */
      Update_Normal_Report_Flag(hr, spo2, valid, alert_code);

      /* 5. 保留串口调试输出。后续语音/蓝牙模块如果需要数据，直接读取 g_health_data。 */
      Output_Data_If_Need();
    }
    else
    {
    //   printf("measure error\r\n");
    }

    rt_thread_delay(500);

  }
}
