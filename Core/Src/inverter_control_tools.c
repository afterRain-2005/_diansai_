#include "inverter_control_tools.h"
#include "Bipolar_PWM.h"
#include "main.h"
#include <math.h>

extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim1;

volatile int32_t INV_Freq = INV_FREQ_DEFAULT;
volatile int32_t INV_Amp = INV_AMP_DEFAULT;
volatile int32_t INV_PwmFlag;
volatile int32_t INV_LoopEnable;
volatile int32_t INV_FeedbackChannel;
volatile float INV_TargetVoltage = 0.0f;
volatile float INV_Vrms = -1.0f;

/* RMS 窗口按名义 ADC 触发率随频率调整，覆盖最低 30 Hz 的整周窗口。 */
#define RMS_MAX_SAMPLES ((INV_TIM_CLK / INV_PERIOD + INV_FREQ_MIN - 1U) / INV_FREQ_MIN)
static float rms_squares[RMS_MAX_SAMPLES];
static uint32_t rms_window_size = INV_RMS_WINDOW_SIZE;
static uint32_t rms_index, rms_count;
/* 必须用 float：M4F 的 FPU 只支持单精度，double 是软件模拟，
 * 20kHz 采样中断里用 double 会吃满 CPU 导致系统假死。
 */
static float rms_square_sum;
static float voltage_integral;
static uint32_t voltage_pi_initialized;
static uint32_t voltage_pi_tick;

/* 调用方已屏蔽中断，只执行状态复位，避免临界区重复嵌套。 */
static void ResetVoltageControl(void)
{
    rms_index = 0U;
    rms_count = 0U;
    rms_square_sum = 0.0f;
    voltage_integral = 0.0f;
    voltage_pi_initialized = 0U;
    voltage_pi_tick = HAL_GetTick();
    INV_Vrms = -1.0f;
}

void INV_ResetVoltageControl(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ResetVoltageControl();
    __set_PRIMASK(primask);
}

uint32_t INV_GetLoopState(void)
{
    if (!INV_LoopEnable) return INV_LOOP_OPEN;
    if (INV_TargetVoltage <= 0.0f ||
        (INV_VOLTAGE_KP == 0.0f && INV_VOLTAGE_KI == 0.0f))
        return INV_LOOP_SETUP;
    return INV_Vrms < 0.0f ? INV_LOOP_WAIT : INV_LOOP_READY;
}

void INV_SetLoopEnable(int32_t enable)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    INV_LoopEnable = enable != 0;
    ResetVoltageControl();
    __set_PRIMASK(primask);
}

void INV_SetFeedbackChannel(int32_t channel)
{
    if (channel < 0 || channel > 1) return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    INV_FeedbackChannel = channel;
    ResetVoltageControl();
    __set_PRIMASK(primask);
}

void INV_SetTargetVoltage(float target)
{
    if (target < 0.0f) target = 0.0f;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    INV_TargetVoltage = target;
    ResetVoltageControl();
    __set_PRIMASK(primask);
}

float INV_GetVoltageRms(void)
{
    return INV_Vrms;
}

/* 每个采样帧调用一次：滑动窗口 O(1) 更新 RMS；PI 至多 1ms 执行一次，
 * dt 取真实经过时间。仅闭环 READY 状态下调幅。
 */
void INV_VoltageControlTick(float voltage)
{
    INV_Vrms = INV_CalculateVoltageRms(voltage);
    uint32_t now = HAL_GetTick();
    if (!INV_PwmFlag || INV_GetLoopState() != INV_LOOP_READY)
    {
        voltage_pi_tick = now;
        voltage_pi_initialized = 0U;
        return;
    }
    uint32_t elapsed = now - voltage_pi_tick;
    if (elapsed >= 1U)
    {
        voltage_pi_tick = now;
        INV_VoltagePI(INV_TargetVoltage, INV_Vrms, INV_VOLTAGE_KP,
                      INV_VOLTAGE_KI, (float)elapsed * 0.001f);
    }
}

float INV_CalculateVoltageRms(float voltage)
{
    float square = voltage * voltage;

    /* 窗口未满时忽略旧数组内容，因此复位无需清空整个缓冲区。
     * float 累加：电压平方约百量级，整周求和远小于 2^24，精度足够
     * 且全部由硬件 FPU 完成，满足 20kHz 中断的实时预算。
     */
    if (rms_count == rms_window_size)
        rms_square_sum -= rms_squares[rms_index];
    else
        rms_count++;
    rms_squares[rms_index] = square;
    rms_square_sum += square;
    rms_index = (rms_index + 1U) % rms_window_size;

    if (rms_count < rms_window_size) return -1.0f;
    if (rms_square_sum < 0.0f) rms_square_sum = 0.0f;
    return sqrtf(rms_square_sum / (float)rms_window_size);
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
    float rms = INV_CalculateVoltageRms(voltage);
    if (rms != -1.0f)
        INV_VoltagePI(target_rms, rms, kp, ki, dt);
    return rms;
}

void INV_SetFrequency(int32_t frequency)
{
    if (frequency < INV_FREQ_MIN) frequency = INV_FREQ_MIN;
    if (frequency > INV_FREQ_MAX) frequency = INV_FREQ_MAX;

    uint32_t rate = INV_SAMP_NUMBER * (uint32_t)frequency;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    INV_Freq = frequency;
    /* RMS 窗口覆盖一个基波周期，改变频率后旧窗口不再有效。 */
    rms_window_size = (INV_TIM_CLK / INV_PERIOD + (uint32_t)frequency / 2U) /
                      (uint32_t)frequency;
    __HAL_TIM_SET_AUTORELOAD(&htim5, (INV_TIM_CLK + rate / 2U) / rate - 1U);
    ResetVoltageControl();
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
    /* 闭环已使能但目标/增益未配置时拒绝启动，防止无目标开输出。 */
    if (INV_GetLoopState() == INV_LOOP_SETUP) return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ResetVoltageControl();
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
    ResetVoltageControl();
    __set_PRIMASK(primask);
}
