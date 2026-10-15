#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "inverter_mock/main.h"
#include "../Core/Src/Bipolar_PWM.c"
#include "../Core/Src/inverter_menu.c"
#include "../Core/Src/oled.c"
#include "../Core/Src/Keyboard.c"

GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_d, test_gpio_e;
TIM_TypeDef test_tim1, test_tim5;
ADC_TypeDef test_adc;
RCC_TypeDef test_rcc;
FLASH_TypeDef test_flash;
SYSCFG_TypeDef test_syscfg;
uint32_t SystemCoreClock = INV_TIM_CLK;
uint32_t test_flash_size = 0x80000U;
TIM_HandleTypeDef htim1 = {TIM1}, htim5 = {TIM5};
ADC_HandleTypeDef hadc1 = {ADC1};
SPI_HandleTypeDef hspi2;
static uint32_t test_primask, test_tick, test_flash_calls, test_fail_at;
static HAL_StatusTypeDef test_adc_status, test_spi_status;
static uint16_t *test_dma;
static uint32_t test_spi_calls;
static uint8_t test_pixels[OLED_PAGES][OLED_WIDTH];
static uint32_t test_pixel_page;

uint32_t __get_PRIMASK(void) { return test_primask; }
void __disable_irq(void) { test_primask = 1U; }
void __set_PRIMASK(uint32_t value) { test_primask = value; }
uint32_t HAL_GetTick(void) { return test_tick; }
void HAL_Delay(uint32_t delay) { test_tick += delay; }
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
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *handle, uint32_t mode)
{
    assert(handle == &hadc1 && mode == ADC_SINGLE_ENDED);
    return test_adc_status;
}
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *handle, uint32_t *data, uint32_t length)
{
    assert(handle == &hadc1 && length == 2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS);
    test_dma = (uint16_t *)data;
    return test_adc_status;
}
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *handle)
{
    assert(handle == &hadc1 && !INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    return test_adc_status;
}
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *error)
{
    uint32_t address = 0x08000000U;
    uint32_t page_size = (FLASH->OPTR & FLASH_OPTR_DBANK) ? 2048U : 4096U;

    assert(!INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    if (page_size == 2048U)
    {
        int upper = (erase->Banks == FLASH_BANK_2) !=
                    ((SYSCFG->MEMRMP & SYSCFG_MEMRMP_FB_MODE) != 0U);
        if (upper) address += 0x40000U;
    }
    address += erase->Page * page_size;
    assert(address >= INV_FLASH_ADDR && address + erase->NbPages * page_size <= 0x08080000U);
    *error = 0xFFFFFFFFU;
    test_flash_calls++;
    if (test_flash_calls == test_fail_at) return HAL_ERROR;
    memset((void *)(uintptr_t)address, 0xFF, erase->NbPages * page_size);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint64_t word)
{
    (void)type;
    assert(address >= INV_FLASH_ADDR && address + 8U <= 0x08080000U && address % 8U == 0U);
    assert(*(uint64_t *)(uintptr_t)address == UINT64_MAX);
    test_flash_calls++;
    if (test_flash_calls == test_fail_at)
    {
        memcpy((void *)(uintptr_t)address, &word, 4U);
        return HAL_ERROR;
    }
    memcpy((void *)(uintptr_t)address, &word, sizeof(word));
    return HAL_OK;
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

static void ResetFixture(void)
{
    memset(TIM1, 0, sizeof(*TIM1));
    memset(TIM5, 0, sizeof(*TIM5));
    memset(GPIOA, 0, sizeof(*GPIOA));
    memset(GPIOB, 0, sizeof(*GPIOB));
    RCC->APB2ENR = RCC_APB2ENR_TIM1EN;
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    TIM1->ARR = INV_PERIOD - 1U;
    GPIOA->IDR = INV_BKIN_Pin;
    INV_FaultFlags = 0U;
    INV_BreakFlag = 0;
    initialized = 0U;
    sampling_paused = 0U;
    adc_ready = 0U;
    INV_Freq = INV_FREQ_DEFAULT;
    INV_Amp = INV_AMP_DEFAULT;
    test_adc_status = HAL_OK;
    test_spi_status = HAL_OK;
    test_tick = 0U;
    test_primask = 0U;
    test_fail_at = 0U;
    test_flash_calls = 0U;
    minimum_compare = 0U;
    INV_OutputDisable();
    assert(INV_Init() == HAL_OK);
    assert(!INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    assert((TIM1->BDTR & 255U) == 0x95U);
}

static void FreshAdc(void)
{
    for (uint32_t index = 0U; index < 2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS; index++)
        test_dma[index] = 2048U;
    HAL_ADC_ConvHalfCpltCallback(&hadc1);
    HAL_TIM_PeriodElapsedCallback(&htim5);
}

static void CheckMath(void)
{
    uint32_t code, bank, page, size;
    assert(INV_EncodeDeadTime(1000, INV_TIM_CLK, &code) && code == 0x95U);
    assert(!INV_EncodeDeadTime(0, INV_TIM_CLK, &code));
    assert(!INV_EncodeDeadTime(-1, INV_TIM_CLK, &code));
    assert(!INV_EncodeDeadTime(6000, INV_TIM_CLK, &code));
    for (int32_t ns = 1; ns < 5929; ns++)
    {
        assert(INV_EncodeDeadTime(ns, INV_TIM_CLK, &code));
        assert((uint64_t)INV_DeadTimeTicks(code) * 1000000000ULL >= (uint64_t)ns * INV_TIM_CLK);
        assert(code == 0U || (uint64_t)INV_DeadTimeTicks(code - 1U) * 1000000000ULL < (uint64_t)ns * INV_TIM_CLK);
    }
    assert(INV_Crc32("123456789", 9U) == 0xCBF43926U);
    assert(INV_FlashPage(0x0807E000U, 0x80000U, 1, 0, &bank, &page, &size));
    assert(bank == 2U && page == 124U && size == 2048U);
    assert(INV_FlashPage(0x0807F000U, 0x80000U, 1, 1, &bank, &page, &size));
    assert(bank == 1U && page == 126U);
    assert(INV_FlashPage(0x0807E000U, 0x80000U, 0, 0, &bank, &page, &size));
    assert(bank == 1U && page == 126U && size == 4096U);
    assert(!INV_FlashPage(0x08080000U, 0x80000U, 1, 0, &bank, &page, &size));
    assert(INV_RmsFromSums(4095ULL * 4095U * 1667U, 4095ULL * 1667U, 1667U, 1) == 0.0f);
    assert(INV_RmsFromSums(4095ULL * 4095U * 1667U, 4095ULL * 1667U, 1667U, 0) == 4095.0f);
    assert(fabsf(INV_RmsFromSums(4095ULL * 4095U * 500U, 4095ULL * 500U, 1000U, 1) - 2047.5f) < 0.01f);
}

static void CheckControl(void)
{
    uint32_t duties[3][INV_SAMP_NUMBER];
    ResetFixture();
    INV_SetFrequency(INT32_MAX);
    assert(INV_Freq == INV_FREQ_MAX);
    INV_SetFrequency(INT32_MIN);
    assert(INV_Freq == INV_FREQ_MIN);
    INV_SetFrequency(INV_FREQ_DEFAULT);
    assert(INV_OutputEnable() == HAL_ERROR);
    FreshAdc();
    assert(INV_OutputEnable() == HAL_OK);
    assert(INV_SetDeadTime(2000) == HAL_BUSY);
    for (uint32_t index = 0; index < 10U; index++) HAL_TIM_PeriodElapsedCallback(&htim5);
    assert(active_amplitude > 0U && active_amplitude < 1000U);
    active_amplitude = 50000U;
    phase_index = 0U;
    for (uint32_t index = 0; index < INV_SAMP_NUMBER; index++)
    {
        Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
        for (uint32_t channel = 0; channel < 3U; channel++)
        {
            duties[channel][index] = TIM1->compare[channel];
            assert(duties[channel][index] >= minimum_compare);
            assert(duties[channel][index] <= INV_PERIOD - minimum_compare);
        }
    }
    for (uint32_t index = 0; index < INV_SAMP_NUMBER; index++)
    {
        assert(duties[1][index] == duties[0][(index + 120U) % 360U]);
        assert(duties[2][index] == duties[0][(index + 240U) % 360U]);
    }
    test_primask = 1U;
    INV_OutputDisable();
    assert(test_primask == 1U);
    test_primask = 0U;
    assert(!(TIM1->BDTR & TIM_BDTR_MOE) && !INV_PwmFlag);
    assert(GPIOA->BSRR == ((uint32_t)PWM_EN_Pin << 16U | INV_OFFLINE_Pin));
    INV_SetAmplitude(INT32_MAX);
    assert(INV_Amp == 88);
    INV_SetAmplitude(INT32_MIN);
    assert(INV_Amp == 0);
    assert(INV_SetDeadTime(0) == HAL_ERROR);
    assert(INV_SetDeadTime(6000) == HAL_ERROR);
    assert(INV_SetDeadTime(2000) == HAL_OK && !(TIM1->BDTR & TIM_BDTR_MOE));
    ResetFixture();
    FreshAdc();
    assert(INV_OutputEnable() == HAL_OK);
    GPIOA->IDR = 0U;
    HAL_TIMEx_BreakCallback(&htim1);
    assert(INV_FaultFlags == INV_FAULT_BREAK && !INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    assert(!(TIM1->DIER & TIM_IT_BREAK));
    assert(INV_ClearFault() == HAL_ERROR && INV_OutputEnable() == HAL_ERROR);
    GPIOA->IDR = INV_BKIN_Pin;
    assert(INV_OutputEnable() == HAL_ERROR);
    assert(INV_ClearFault() == HAL_OK && !INV_PwmFlag);
    assert(INV_OutputEnable() == HAL_OK);
    test_tick += INV_ADC_TIMEOUT_MS + 1U;
    INV_SafetyTick();
    assert(INV_FaultFlags == INV_FAULT_TIMEOUT && !INV_PwmFlag);
    FreshAdc();
    assert(INV_ClearFault() == HAL_OK);
    assert(INV_OutputEnable() == HAL_OK);
    HAL_ADC_ErrorCallback(&hadc1);
    assert(INV_FaultFlags == INV_FAULT_ADC && !INV_PwmFlag);
    assert(INV_ClearFault() == HAL_ERROR);
    ResetFixture();
    assert(PI_Calculate(100, 200, 1000.0f, 0.0f) == 100);
    assert(PI_Calculate(200, 100, 1000.0f, 0.0f) == -100);
    assert(PI_Calculate(0, 65535U, 3.4e38f, 0.0f) == 0);
    FreshAdc();
    assert(INV_OutputEnable() == HAL_OK);
    INV_SetAmplitude(50);
    Vout_Ctr(200, 100, 1000.0f, 0.0f);
    assert(INV_Amp == 49);
    INV_EmergencyStop();
    assert(INV_FaultFlags == INV_FAULT_CPU && !INV_PwmFlag);
    ResetFixture();
    Bipolar_pwm(INV_PERIOD, INT32_MAX);
    assert(INV_FaultFlags == INV_FAULT_INIT);
    ResetFixture();
    test_adc_status = HAL_ERROR;
    assert(INV_Init() == HAL_ERROR && !INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
}

static void CheckAdcPipeline(void)
{
    ResetFixture();
    for (uint32_t frame = 0U; frame < 2U * INV_ADC_HALF_FRAMES; frame++)
    {
        test_dma[frame * 4U] = frame % 2U ? 4095U : 0U;
        test_dma[frame * 4U + 1U] = 123U;
        test_dma[frame * 4U + 2U] = 234U;
        test_dma[frame * 4U + 3U] = 345U;
    }
    for (uint32_t block = 0U; block < 64U; block++)
    {
        HAL_ADC_ConvHalfCpltCallback(&hadc1);
        HAL_ADC_ConvCpltCallback(&hadc1);
    }
    assert(INV_AdcVrms == 2048 && INV_Vrms == 165);
    assert(INV_AdcCh[1] == 123 && INV_AdcCh[2] == 234 && INV_AdcCh[3] == 345);
    for (uint32_t index = 0U; index < 2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS; index++)
        test_dma[index] = 4095U;
    INV_SetFrequency(INV_FREQ_MIN);
    for (uint32_t block = 0U; block < 120U; block++)
    {
        HAL_ADC_ConvHalfCpltCallback(&hadc1);
        HAL_ADC_ConvCpltCallback(&hadc1);
    }
    assert(INV_AdcVrms == 0 && INV_Vrms == 0);
}

static void CheckFlash(void)
{
    INV_PARAM_T record;
    uint8_t backup[2U * INV_FLASH_SLOT_SIZE];
    assert(VirtualAlloc((void *)0x08070000U, 0x10000U, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != NULL);
    for (uint32_t mode = 0; mode < 3U; mode++)
    {
        ResetFixture();
        FLASH->OPTR = mode == 0U ? 0U : FLASH_OPTR_DBANK;
        SYSCFG->MEMRMP = mode == 2U ? SYSCFG_MEMRMP_FB_MODE : 0U;
        memset((void *)INV_FLASH_ADDR, 0xFF, sizeof(backup));
        assert(ReadLatest(&record) == -1);
        assert(INV_Menu_Save() == HAL_OK);
        assert(ReadLatest(&record) == 0 && record.amplitude == INV_AMP_DEFAULT);
        memcpy(backup, (const void *)INV_FLASH_ADDR, sizeof(backup));
        for (uint32_t failure = 1; failure <= 5U; failure++)
        {
            memcpy((void *)INV_FLASH_ADDR, backup, sizeof(backup));
            INV_SetAmplitude(20);
            test_flash_calls = 0U;
            test_fail_at = failure;
            assert(INV_Menu_Save() == HAL_ERROR);
            assert(!INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
            assert(ReadLatest(&record) == 0 && record.amplitude == INV_AMP_DEFAULT);
        }
        memcpy((void *)INV_FLASH_ADDR, backup, sizeof(backup));
        test_fail_at = 0U;
        test_flash_calls = 0U;
        assert(INV_Menu_Save() == HAL_OK);
        assert(ReadLatest(&record) == 1 && record.amplitude == 20);
        uint32_t calls_before = test_flash_calls;
        assert(INV_Menu_Save() == HAL_OK && test_flash_calls == calls_before);
        INV_Menu_Init();
        assert(INV_Amp == 20);
        ((INV_PARAM_T *)(INV_FLASH_ADDR + INV_FLASH_SLOT_SIZE))->amplitude ^= 1;
        assert(ReadLatest(&record) == 0);
    }
    char formatted[16];
    FormatValue(formatted, sizeof(formatted), INT32_MIN, 1);
    assert(strcmp(formatted, "-214748364.8") == 0);
    assert(VirtualFree((void *)0x08070000U, 0U, MEM_RELEASE));
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
    CheckMath();
    CheckControl();
    CheckAdcPipeline();
    CheckFlash();
    CheckOled();
    puts("inverter_check: PASS (deadtime, PWM, faults, RMS, Flash power loss, OLED bounds)");
    return 0;
}
