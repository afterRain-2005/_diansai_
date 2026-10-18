#include "inverter_control_tools.h"
#include "Bipolar_PWM.h"
#include <math.h>

extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim1;

/* RMS 采样率 = TIM1 更新事件频率：16 MHz、中心对齐 ARR=800，上下溢出各触发一次。 */
#define INV_SAMPLE_RATE     (INV_TIM_CLK / INV_PERIOD)
/* 最低频率 30 Hz 时窗口最长（20000/30 ≈ 667 点），环形缓冲按最长窗口开。 */
#define INV_RMS_WINDOW_MAX  ((INV_SAMPLE_RATE + INV_FREQ_MIN / 2U) / INV_FREQ_MIN)

#define INV_VOLTAGE_KP      2.0f  /* %/V */
#define INV_VOLTAGE_KI      20.0f /* %/(V*s) */

volatile int32_t INV_Freq = INV_FREQ_DEFAULT;
volatile int32_t INV_Amp = INV_AMP_DEFAULT;
volatile int32_t INV_PwmFlag;

volatile float INV_TargetVoltage = 0.0f;

static float rms_squares[INV_RMS_WINDOW_MAX];
static uint32_t rms_window = INV_RMS_WINDOW_SIZE;
static float rms_window_recip = 1.0f / (float)INV_RMS_WINDOW_SIZE;
static uint32_t rms_index, rms_count;
static double rms_square_sum;
static volatile float control_dt = 1.0f / (float)INV_FREQ_DEFAULT;
static float voltage_pi_last_error;
static uint32_t voltage_pi_initialized;
static uint32_t control_tick_count;
static volatile float voltage_rms_cached = -1.0f;

void INV_SetTargetVoltage(float target)
{
    if (target < 0.0f) target = 0.0f;
    INV_TargetVoltage = target;
}

float INV_GetVoltageRms(void)
{
    return voltage_rms_cached;
}

void INV_GetParams(INV_Params *params)
{
    if (params == NULL) return;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    params->frequency = INV_Freq;
    params->target_voltage = INV_TargetVoltage;
    params->amplitude = INV_Amp;
    params->voltage_rms = voltage_rms_cached;
    params->output_enabled = INV_PwmFlag;
    __set_PRIMASK(primask);
}

/* 每个采样帧调用一次：采样平方和必须逐点累加，才能还原一个基波周期的有效值。
 * 目标为 0 或输出关闭时只更新实测值供显示，不改幅值。
 */
void INV_VoltageControlTick(float voltage)
{
    float rms = (INV_PwmFlag && INV_TargetVoltage > 0.0f)
                    ? INV_UpdateVoltageControl(voltage, INV_TargetVoltage,
                                               INV_VOLTAGE_KP, INV_VOLTAGE_KI,
                                               control_dt)
                    : INV_CalculateVoltageRms(voltage);
    if (rms >= 0.0f) voltage_rms_cached = rms;
}

void INV_ResetVoltageControl(void)
{
    rms_index = 0U;
    rms_count = 0U;
    rms_square_sum = 0.0;
    voltage_pi_last_error = 0.0f;
    voltage_pi_initialized = 0U;
    control_tick_count = 0U;
    voltage_rms_cached = -1.0f;
}

float INV_CalculateVoltageRms(float voltage)
{
    float square = voltage * voltage;

    /* 窗口未满时忽略旧数组内容，因此复位无需清空整个缓冲区。
     * 使用 double 累加，减小长时间滑动加减的舍入误差。
     * 窗口长度随基波频率变化，等于名义采样率除以频率。
     */
    if (rms_count == rms_window)
        rms_square_sum -= rms_squares[rms_index];
    else
        rms_count++;
    rms_squares[rms_index] = square;
    rms_square_sum += square;
    if (++rms_index >= rms_window) rms_index = 0U;

    if (rms_count < rms_window) return -1.0f;
    /* 每样本都会走这里，用预存的浮点倒数代替 double 除法，省掉 __aeabi_ddiv。 */
    return sqrtf(fmaxf((float)rms_square_sum, 0.0f) * rms_window_recip);
}

void INV_VoltagePI(float target_rms, float measured_rms,
                   float kp, float ki, float dt)
{
    if (!INV_PwmFlag)
    {
        voltage_pi_initialized = 0U;
        return;
    }
    if (target_rms < 0.0f || measured_rms < 0.0f ||
        kp < 0.0f || ki < 0.0f || dt <= 0.0f) return;

    /* 增量式：Δu = Kp*(e-e_prev) + Ki*e*dt，叠在上一次幅值上。
     * 没有积分累加器，幅值被钳位后不会积攒反向退出量；首次调用取 e_prev=0。
     */
    float error = target_rms - measured_rms;
    float previous_error = voltage_pi_initialized ? voltage_pi_last_error : 0.0f;
    float output = (float)INV_Amp + kp * (error - previous_error) + ki * error * dt;
    output = fminf(fmaxf(output, (float)INV_AMP_MIN), (float)INV_AMP_MAX);

    voltage_pi_last_error = error;
    voltage_pi_initialized = 1U;
    INV_SetAmplitude((int32_t)(output + 0.5f));
}

float INV_UpdateVoltageControl(float voltage, float target_rms,
                               float kp, float ki, float dt)
{
    /* 每样本只累加平方和（O(1)），满一个窗口才开方并调一次 PI。
     * 计数必须先加，窗口填满前的样本也要算进本窗口。
     */
    control_tick_count++;
    float rms = INV_CalculateVoltageRms(voltage);
    if (rms < 0.0f) return -1.0f;

    if (control_tick_count < rms_window) return rms;
    control_tick_count = 0U;
    INV_VoltagePI(target_rms, rms, kp, ki, dt);
    return rms;
}

void INV_SetFrequency(int32_t frequency)
{
    if (frequency < INV_FREQ_MIN) frequency = INV_FREQ_MIN;
    if (frequency > INV_FREQ_MAX) frequency = INV_FREQ_MAX;

    uint32_t rate = INV_SAMP_NUMBER * (uint32_t)frequency;
    uint32_t window = (INV_SAMPLE_RATE + (uint32_t)frequency / 2U) / (uint32_t)frequency;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    INV_Freq = frequency;
    __HAL_TIM_SET_AUTORELOAD(&htim5, (INV_TIM_CLK + rate / 2U) / rate - 1U);
    control_dt = 1.0f / (float)frequency;
    if (window != rms_window)
    {
        /* 窗口长度变了，旧样本按新长度滑动会串窗，清空后等新窗口填满。 */
        rms_window = window;
        rms_window_recip = 1.0f / (float)window;
        INV_ResetVoltageControl();
    }
    __set_PRIMASK(primask);
}

void INV_SetAmplitude(int32_t amplitude)
{
    if (amplitude < INV_AMP_MIN) amplitude = INV_AMP_MIN;
    if (amplitude > INV_AMP_MAX) amplitude = INV_AMP_MAX;
    INV_Amp = amplitude;
}

void INV_OutputEnable(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    /* 先装入目标占空比，再开启输出。 */
    INV_PwmFlag = 1;
    Bipolar_pwm();
    TIM1->EGR = TIM_EGR_UG;
    SET_BIT(TIM1->BDTR, TIM_BDTR_MOE);
    HAL_GPIO_WritePin(INV_OFFLINE_GPIO_Port, INV_OFFLINE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS_CTRL_GPIO_Port, RS_CTRL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PWM_EN_GPIO_Port, PWM_EN_Pin, GPIO_PIN_SET);
    __set_PRIMASK(primask);
}

void INV_OutputDisable(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    /* 错误入口可能在外设初始化前调用，未开时钟时不访问寄存器。 */
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U)
        CLEAR_BIT(TIM1->BDTR, TIM_BDTR_MOE);
    if ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN) != 0U)
        GPIOA->BSRR = ((uint32_t)PWM_EN_Pin << 16U) | INV_OFFLINE_Pin;
    if ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOBEN) != 0U)
        GPIOB->BSRR = (uint32_t)RS_CTRL_Pin << 16U;
    INV_PwmFlag = 0;
    INV_ResetVoltageControl();
    __set_PRIMASK(primask);
}
