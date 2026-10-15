#include "inverter_control_tools.h"
#include "Bipolar_PWM.h"

extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim1;

volatile int32_t INV_Freq = INV_FREQ_DEFAULT;
volatile int32_t INV_Amp = INV_AMP_DEFAULT;
volatile int32_t INV_PwmFlag;

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
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
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
    __set_PRIMASK(primask);
}
