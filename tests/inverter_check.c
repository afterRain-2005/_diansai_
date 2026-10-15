#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "inverter_mock/main.h"
#include "../Core/Src/Bipolar_PWM.c"
#include "../Core/Src/inverter_control_tools.c"
#include "../Core/Src/inverter_menu.c"
#include "../Core/Src/oled.c"

GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_d, test_gpio_e;
TIM_TypeDef test_tim1, test_tim5;
RCC_TypeDef test_rcc;
TIM_HandleTypeDef htim1 = {TIM1}, htim5 = {TIM5};
SPI_HandleTypeDef hspi2;
static uint32_t test_primask, test_tick, test_spi_calls;
static HAL_StatusTypeDef test_spi_status;
static uint8_t test_pixels[OLED_PAGES][OLED_WIDTH];
static uint32_t test_pixel_page;
static const char *test_keys;

uint32_t __get_PRIMASK(void) { return test_primask; }
void __disable_irq(void) { test_primask = 1U; }
void __set_PRIMASK(uint32_t value) { test_primask = value; }
uint32_t HAL_GetTick(void) { return test_tick; }
void HAL_Delay(uint32_t delay) { test_tick += delay; }
char Keyboard_GetKey(void) { assert(*test_keys); return *test_keys++; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    (void)port;
    (void)config;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, uint32_t state)
{
    if (state == GPIO_PIN_SET) port->ODR |= pins;
    else port->ODR &= ~(uint32_t)pins;
}
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pins)
{
    return (port->IDR & pins) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *handle)
{
    handle->Instance->CR1 |= TIM_CR1_CEN;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *handle)
{
    handle->Instance->DIER |= TIM_IT_UPDATE;
    return HAL_TIM_Base_Start(handle);
}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *handle, uint8_t *data, uint16_t length, uint32_t timeout)
{
    assert(handle == &hspi2 && timeout <= 5U);
    test_spi_calls++;
    if (length == 128U)
    {
        memcpy(test_pixels[test_pixel_page % OLED_PAGES], data, 128U);
        test_pixel_page++;
    }
    return test_spi_status;
}


static void CheckControl(void)
{
    uint32_t duties[3][INV_SAMP_NUMBER];
    RCC->APB2ENR = RCC_APB2ENR_TIM1EN;
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    TIM1->ARR = INV_PERIOD;
    TIM1->BDTR = 149U;
    assert(INV_Init() == HAL_OK);
    assert(!INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    assert((TIM1->BDTR & 255U) == 149U);
    assert(TIM5->ARR == 443U);
    assert(TIM1->CR1 & TIM_CR1_CEN);
    assert(TIM5->DIER & TIM_IT_UPDATE);

    INV_SetFrequency(INT32_MAX);
    assert(INV_Freq == INV_FREQ_MAX && TIM5->ARR == 277U);
    INV_SetFrequency(INT32_MIN);
    assert(INV_Freq == INV_FREQ_MIN && TIM5->ARR == 740U);
    INV_SetAmplitude(INT32_MAX);
    assert(INV_Amp == INV_AMP_MAX);
    INV_OutputEnable();
    assert(INV_PwmFlag && (TIM1->BDTR & TIM_BDTR_MOE));
    assert(GPIOA->ODR & PWM_EN_Pin);
    assert(!(GPIOA->ODR & INV_OFFLINE_Pin));
    assert(GPIOB->ODR & RS_CTRL_Pin);

    for (uint32_t i = 0; i < INV_SAMP_NUMBER; i++)
    {
        HAL_TIM_PeriodElapsedCallback(&htim5);
        for (uint32_t ch = 0; ch < 3U; ch++)
        {
            duties[ch][i] = TIM1->compare[ch];
            assert(duties[ch][i] <= INV_PERIOD);
        }
    }
    for (uint32_t i = 0; i < INV_SAMP_NUMBER; i++)
    {
        assert(duties[1][i] == duties[0][(i + 120U) % 360U]);
        assert(duties[2][i] == duties[0][(i + 240U) % 360U]);
    }
    uint32_t minimum = INV_PERIOD, maximum = 0;
    uint64_t squares = 0;
    for (uint32_t i = 0; i < INV_SAMP_NUMBER; i++)
    {
        int32_t centered = (int32_t)duties[0][i] - INV_PERIOD / 2;
        if (duties[0][i] < minimum) minimum = duties[0][i];
        if (duties[0][i] > maximum) maximum = duties[0][i];
        squares += (uint64_t)(centered * centered);
    }
    assert(minimum <= 1U && maximum >= INV_PERIOD - 1U);
    assert(squares / INV_SAMP_NUMBER > 19500U && squares / INV_SAMP_NUMBER < 20500U);

    INV_SetAmplitude(INT32_MIN);
    assert(INV_Amp == 0);
    HAL_TIM_PeriodElapsedCallback(&htim5);
    for (uint32_t ch = 0; ch < 3U; ch++) assert(TIM1->compare[ch] == INV_PERIOD / 2);
    test_primask = 1U;
    INV_OutputDisable();
    assert(test_primask == 1U && !INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    assert(GPIOA->BSRR == ((uint32_t)PWM_EN_Pin << 16U | INV_OFFLINE_Pin));
    assert(GPIOB->BSRR == (uint32_t)RS_CTRL_Pin << 16U);
    test_primask = 0U;
    INV_SetAmplitude(50);
    HAL_TIM_PeriodElapsedCallback(&htim5);
    for (uint32_t ch = 0; ch < 3U; ch++) assert(TIM1->compare[ch] == INV_PERIOD / 2);
    SET_BIT(TIM1->CR1, TIM_CR1_UDIS);
    Bipolar_pwm();
    assert(TIM1->CR1 & TIM_CR1_UDIS);
    CLEAR_BIT(TIM1->CR1, TIM_CR1_UDIS);
}

static void CheckMenu(void)
{
    INV_SetFrequency(50);
    INV_SetAmplitude(10);
    INV_OutputEnable();
    test_keys = "6826D";
    INV_Menu_Run();
    assert(INV_Freq == 52 && INV_Amp == 10 && !INV_PwmFlag);
    test_keys = "86C";
    INV_Menu_Run();
    assert(INV_Amp == 11 && !INV_PwmFlag);
    INV_ShowStandby();
}

static void CheckOled(void)
{
    OLED_Clear();
    OLED_DrawPoint(0, 0, 1);
    OLED_DrawRect(128, 64, 255, 255, 0);
    OLED_DrawRect(0, 0, 0, 0, 0);
    OLED_ShowChar(255, 255, 'A', 16);
    assert(oled_buf[0][0] == 1U);
    OLED_DrawRect(126, 62, 255, 255, 1);
    assert((oled_buf[7][127] & 0xC0U) == 0xC0U);
    OLED_Clear();
    OLED_ShowString(0, 0, NULL, 16);
    OLED_ShowString(120, 0, "AAAAAAAAAAAAAAAAAAAAAAAAAAAA", 16);
    for (uint32_t index = 0; index < 120U; index++) assert(oled_buf[2][index] == 0U);
    uint8_t letter[sizeof(oled_buf)];
    OLED_Clear();
    OLED_ShowChar(0, 0, 'a', 16);
    memcpy(letter, oled_buf, sizeof(letter));
    OLED_Clear();
    OLED_ShowChar(0, 0, 'A', 16);
    assert(memcmp(letter, oled_buf, sizeof(letter)) == 0);
    test_spi_calls = 0U;
    test_spi_status = HAL_TIMEOUT;
    OLED_Refresh();
    assert(test_spi_calls == 1U);
    test_spi_status = HAL_OK;
    test_pixel_page = 0U;
    OLED_Refresh();
    assert(test_pixel_page == 8U);
}

int main(void)
{
    CheckControl();
    CheckMenu();
    CheckOled();
    puts("inverter_check: PASS (SPWM, frequency, amplitude, output, menu, OLED)");
    return 0;
}
