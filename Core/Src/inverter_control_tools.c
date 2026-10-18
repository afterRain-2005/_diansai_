#include "inverter_control_tools.h"
#include "Bipolar_PWM.h"
#include <math.h>

extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim1;

volatile int32_t INV_Freq = INV_FREQ_DEFAULT;
volatile int32_t INV_Amp = INV_AMP_DEFAULT;
volatile int32_t INV_PwmFlag;

volatile float INV_TargetVoltage = 0.0f;

static float rms_squares[INV_RMS_WINDOW_SIZE];
static uint32_t rms_index, rms_count;
static double rms_square_sum;
static float voltage_integral;
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

/* 每个采样帧调用一次：采样平方和必须逐点累加，才能还原 50 Hz 一周期的有效值。
 * 目标为 0 或输出关闭时只更新实测值供显示，不改幅值。
 */
void INV_VoltageControlTick(float voltage)
{
    float rms = (INV_PwmFlag && INV_TargetVoltage > 0.0f)
                    ? INV_UpdateVoltageControl(voltage, INV_TargetVoltage,
                                               2.0f, 20.0f, 0.02f)
                    : INV_CalculateVoltageRms(voltage);
    if (rms >= 0.0f) voltage_rms_cached = rms;
}

void INV_ResetVoltageControl(void)
{
    rms_index = 0U;
    rms_count = 0U;
    rms_square_sum = 0.0;
    voltage_integral = 0.0f;
    voltage_pi_initialized = 0U;
    control_tick_count = 0U;
    voltage_rms_cached = -1.0f;
}

float INV_CalculateVoltageRms(float voltage)
{
    float square = voltage * voltage;

    /* 窗口未满时忽略旧数组内容，因此复位无需清空整个缓冲区。
     * 使用 double 累加，减小长时间滑动加减的舍入误差。
     */
    if (rms_count == INV_RMS_WINDOW_SIZE)
        rms_square_sum -= rms_squares[rms_index];
    else
        rms_count++;
    rms_squares[rms_index] = square;
    rms_square_sum += square;
    rms_index = (rms_index + 1U) % INV_RMS_WINDOW_SIZE;

    if (rms_count < INV_RMS_WINDOW_SIZE) return -1.0f;
    /* 每样本都会走这里，用浮点倒数代替 double 除法，省掉 __aeabi_ddiv。 */
    return sqrtf(fmaxf((float)rms_square_sum, 0.0f) *
                 (1.0f / (float)INV_RMS_WINDOW_SIZE));
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

    float error = target_rms - measured_rms;
    float proportional = kp * error;
    float integral = voltage_pi_initialized ? voltage_integral : (float)INV_Amp;
    float increment = ki * error * dt;
    float candidate = integral + increment;
    float output = proportional + candidate;

    /* 达到幅值边界时，禁止积分继续向饱和方向增长，允许反向退出。 */
    float previous_output = proportional + integral;
    if (output > INV_AMP_MAX && increment > 0.0f)
    {
        if (previous_output < INV_AMP_MAX)
            integral += INV_AMP_MAX - previous_output;
    }
    else if (output < INV_AMP_MIN && increment < 0.0f)
    {
        if (previous_output > INV_AMP_MIN)
            integral += INV_AMP_MIN - previous_output;
    }
    else
        integral = candidate;
    output = proportional + integral;
    output = fminf(fmaxf(output, (float)INV_AMP_MIN), (float)INV_AMP_MAX);
    voltage_integral = integral;
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

    if (control_tick_count < INV_RMS_WINDOW_SIZE) return rms;
    control_tick_count = 0U;
    INV_VoltagePI(target_rms, rms, kp, ki, dt);
    return rms;
}

void INV_SetFrequency(int32_t frequency)
{
    if (frequency < INV_FREQ_MIN) frequency = INV_FREQ_MIN;
    if (frequency > INV_FREQ_MAX) frequency = INV_FREQ_MAX;

    uint32_t rate = INV_SAMP_NUMBER * (uint32_t)frequency;
    INV_Freq = frequency;
    __HAL_TIM_SET_AUTORELOAD(&htim5, (INV_TIM_CLK + rate / 2U) / rate - 1U);
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
