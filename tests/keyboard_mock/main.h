#ifndef KEYBOARD_TEST_MAIN_H
#define KEYBOARD_TEST_MAIN_H

#include <stdint.h>

#define GPIO_PIN_0 0x01U
#define GPIO_PIN_1 0x02U
#define GPIO_PIN_2 0x04U
#define GPIO_PIN_3 0x08U
#define GPIO_PIN_4 0x10U
#define GPIO_PIN_5 0x20U
#define GPIO_PIN_6 0x40U
#define GPIO_PIN_7 0x80U
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
#define GPIO_MODE_OUTPUT_OD 1U
#define GPIO_MODE_INPUT 2U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define __HAL_RCC_GPIOE_CLK_ENABLE() ((void)0)

typedef struct { uint32_t IDR; uint16_t ODR; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
extern GPIO_TypeDef keyboard_test_gpio;
#define GPIOE (&keyboard_test_gpio)

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, uint32_t state);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);

#endif
