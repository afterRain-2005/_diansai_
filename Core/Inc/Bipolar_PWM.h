#ifndef BIPOLAR_PWM_H
#define BIPOLAR_PWM_H

#include "main.h"

/* HSI 16 MHz，AHB/APB1/APB2 均不分频，TIM8/TIM5 PSC=0。
 * TIM8 中心对齐 ARR=800，载波为 10 kHz。
 * 每个基波周期查表 360 次，TIM5 中断频率为 360*输出频率。
 */
#define INV_TIM_CLK      16000000UL
#define INV_PERIOD       800UL
#define INV_SAMP_NUMBER  360U

HAL_StatusTypeDef INV_Init(void);
/* 建表仅在初始化、TIM5 中断尚未启动时调用。 */
void sin_duty_init(void);
/* 由 TIM5 中断更新三相占空比。 */
void Bipolar_pwm(void);

#endif
