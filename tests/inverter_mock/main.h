#ifndef INVERTER_TEST_MAIN_H
#define INVERTER_TEST_MAIN_H
#define __MAIN_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#define __ALIGNED(value)
#define RESET 0U
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_2 4U
#define GPIO_PIN_3 8U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_6 64U
#define GPIO_PIN_7 128U
#define GPIO_MODE_OUTPUT_OD 1U
#define GPIO_MODE_INPUT 2U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define __HAL_RCC_GPIOE_CLK_ENABLE() ((void)0)
#define RCC_APB2ENR_TIM8EN 0x2000U
#define RCC_AHB2ENR_GPIOAEN 1U
#define RCC_AHB2ENR_GPIOBEN 2U
#define TIM_BDTR_MOE 0x8000U
#define TIM_BDTR_DTG 255U
#define TIM_CR1_CEN 1U
#define TIM_CR1_UDIS 2U
#define TIM_CR1_CKD 0x300U
#define TIM_CR1_CMS 0x60U
#define TIM_CR1_DIR 0x10U
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC1NE 4U
#define TIM_CCER_CC2E 16U
#define TIM_CCER_CC2NE 64U
#define TIM_CCER_CC3E 256U
#define TIM_CCER_CC3NE 1024U
#define TIM_IT_BREAK 128U
#define TIM_IT_UPDATE 1U
#define TIM_FLAG_BREAK TIM_IT_BREAK
#define TIM_FLAG_UPDATE TIM_IT_UPDATE
#define TIM_EGR_UG 1U
#define TIM_CHANNEL_1 0U
#define TIM_CHANNEL_2 1U
#define TIM_CHANNEL_3 2U
#define ADC_SINGLE_ENDED 0U
#define ADC_IT_OVR 1U
#define SET_BIT(reg, bits) ((reg) |= (bits))
#define CLEAR_BIT(reg, bits) ((reg) &= ~(bits))
#define MODIFY_REG(reg, mask, value) ((reg) = ((reg) & ~(mask)) | (value))

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t BSRR, IDR, ODR; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t APB2ENR, AHB2ENR; } RCC_TypeDef;
typedef struct
{
    uint32_t CR1, PSC, ARR, BDTR, CCER, DIER, SR, EGR, compare[3];
} TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { uint32_t interrupts; } ADC_TypeDef;
typedef struct { ADC_TypeDef *Instance; } ADC_HandleTypeDef;
typedef struct { uint32_t unused; } SPI_HandleTypeDef;
typedef struct { uint32_t OPTR; } FLASH_TypeDef;
typedef struct { uint32_t MEMRMP; } SYSCFG_TypeDef;
typedef struct { uint32_t TypeErase, Banks, Page, NbPages; } FLASH_EraseInitTypeDef;

extern GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_d, test_gpio_e;
extern TIM_TypeDef test_tim8, test_tim5;
extern ADC_TypeDef test_adc;
extern RCC_TypeDef test_rcc;
extern FLASH_TypeDef test_flash;
extern SYSCFG_TypeDef test_syscfg;
extern uint32_t SystemCoreClock, test_flash_size;
#define GPIOA (&test_gpio_a)
#define GPIOB (&test_gpio_b)
#define GPIOD (&test_gpio_d)
#define GPIOE (&test_gpio_e)
#define TIM8 (&test_tim8)
#define TIM5 (&test_tim5)
#define ADC1 (&test_adc)
#define RCC (&test_rcc)
#define FLASH (&test_flash)
#define SYSCFG (&test_syscfg)
#define FLASH_SIZE test_flash_size
#define FLASH_OPTR_DBANK 1U
#define SYSCFG_MEMRMP_FB_MODE 1U
#define FLASH_TYPEERASE_PAGES 0U
#define FLASH_TYPEPROGRAM_DOUBLEWORD 0U
#define FLASH_BANK_1 1U
#define FLASH_BANK_2 2U
#define FLASH_FLAG_EOP 1U
#define FLASH_FLAG_SR_ERRORS 2U
#define __HAL_FLASH_CLEAR_FLAG(flags) ((void)(flags))
#define PWM_EN_Pin GPIO_PIN_1
#define PWM_EN_GPIO_Port GPIOA
#define INV_OFFLINE_Pin GPIO_PIN_3
#define INV_OFFLINE_GPIO_Port GPIOA
#define RS_CTRL_Pin GPIO_PIN_6
#define RS_CTRL_GPIO_Port GPIOB
#define OLED_RES_Pin GPIO_PIN_0
#define OLED_RES_GPIO_Port GPIOD
#define OLED_DC_Pin GPIO_PIN_1
#define OLED_DC_GPIO_Port GPIOD
#define OLED_CS_Pin GPIO_PIN_2
#define OLED_CS_GPIO_Port GPIOD
#define __HAL_TIM_DISABLE_IT(handle, bits) CLEAR_BIT((handle)->Instance->DIER, bits)
#define __HAL_TIM_ENABLE_IT(handle, bits) SET_BIT((handle)->Instance->DIER, bits)
#define __HAL_TIM_CLEAR_FLAG(handle, bits) CLEAR_BIT((handle)->Instance->SR, bits)
#define __HAL_TIM_GET_FLAG(handle, bits) ((handle)->Instance->SR & (bits))
#define __HAL_TIM_MOE_ENABLE(handle) SET_BIT((handle)->Instance->BDTR, TIM_BDTR_MOE)
#define __HAL_TIM_SET_AUTORELOAD(handle, value) ((handle)->Instance->ARR = (value))
#define __HAL_TIM_SET_COMPARE(handle, channel, value) ((handle)->Instance->compare[channel] = (value))
#define __HAL_ADC_DISABLE_IT(handle, bits) CLEAR_BIT((handle)->Instance->interrupts, bits)

uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t delay);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, uint32_t state);
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pins);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *handle, uint32_t mode);
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *handle, uint32_t *data, uint32_t length);
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t word);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *handle, uint8_t *data, uint16_t length, uint32_t timeout);

#endif
