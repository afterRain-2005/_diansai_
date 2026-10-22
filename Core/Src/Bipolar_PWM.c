#include "Bipolar_PWM.h"
#include "inverter_control_tools.h"
#include "inverter_sampling.h"
#include <math.h>

extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim5;

static int16_t sine_table[INV_SAMP_NUMBER];
static uint32_t phase_index;

void sin_duty_init(void)
{
    for (uint32_t i = 0; i < INV_SAMP_NUMBER; i++)
        sine_table[i] = (int16_t)(INV_PERIOD * 0.5f *
            sinf(6.28318530718f * (float)i / INV_SAMP_NUMBER));
    phase_index = 0U;
}

void Bipolar_pwm(void)
{
    if (phase_index == 0U) INV_SamplingNotifyCycle();
    /* CCR = ARR/2 + m * ARR/2 * sin(theta)，三相相差 120 度。 */
    int32_t amplitude = INV_PwmFlag ? INV_Amp : 0;
    uint32_t duty[3];

    for (uint32_t phase = 0; phase < 3U; phase++)
    {
        uint32_t index = (phase_index + phase * (INV_SAMP_NUMBER / 3U)) % INV_SAMP_NUMBER;
        duty[phase] = (uint32_t)((int32_t)(INV_PERIOD / 2U) +
                               sine_table[index] * amplitude / 100);
    }

    /* 三相预装载值写完后再允许更新事件。 */
    uint32_t update_disabled = TIM8->CR1 & TIM_CR1_UDIS;
    SET_BIT(TIM8->CR1, TIM_CR1_UDIS);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, duty[0]);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, duty[1]);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, duty[2]);
    if (update_disabled == 0U) CLEAR_BIT(TIM8->CR1, TIM_CR1_UDIS);

    phase_index = (phase_index + 1U) % INV_SAMP_NUMBER;
}

HAL_StatusTypeDef INV_Init(void)
{
    INV_OutputDisable();
    sin_duty_init();
    INV_SetFrequency(INV_FREQ_DEFAULT);
    INV_SetAmplitude(INV_AMP_DEFAULT);
    Bipolar_pwm();
    TIM8->EGR = TIM_EGR_UG;

    /* 定时器持续运行，由 MOE 和驱动使能脚控制输出，死区沿用 CubeMX 配置。 */
    SET_BIT(TIM8->CCER, TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E |
                       TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE);
    if (HAL_TIM_Base_Start(&htim8) != HAL_OK) return HAL_ERROR;
    return HAL_TIM_Base_Start_IT(&htim5);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) Bipolar_pwm();
}
