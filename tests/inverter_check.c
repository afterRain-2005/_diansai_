#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "inverter_mock/main.h"
#include "../Core/Src/Bipolar_PWM.c"
#include "../Core/Src/inverter_control_tools.c"
#include "../Core/Src/inverter_sampling.c"
#include "../Core/Src/inverter_menu.c"
#include "../Core/Src/oled.c"

GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_d, test_gpio_e;
TIM_TypeDef test_tim1, test_tim5;
RCC_TypeDef test_rcc;
TIM_HandleTypeDef htim1 = {TIM1}, htim5 = {TIM5};
ADC_TypeDef test_adc;
ADC_HandleTypeDef hadc1 = {ADC1};
SPI_HandleTypeDef hspi2;
static uint32_t test_primask, test_tick, test_spi_calls;
static HAL_StatusTypeDef test_spi_status;
static uint8_t test_pixels[OLED_PAGES][OLED_WIDTH];
static uint32_t test_pixel_page;
static const char *test_keys;
static HAL_StatusTypeDef test_adc_status;
static uint32_t test_adc_dma_starts;

HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *handle, uint32_t mode)
{
    assert(handle == &hadc1 && mode == ADC_SINGLE_ENDED);
    return test_adc_status;
}
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *handle, uint32_t *data, uint32_t length)
{
    assert(handle == &hadc1 && data == (uint32_t *)sampling_dma && length == 6U);
    test_adc_dma_starts++;
    return test_adc_status;
}
HAL_StatusTypeDef HAL_ADC_Stop_DMA(ADC_HandleTypeDef *handle)
{
    assert(handle == &hadc1);
    return test_adc_status;
}

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
    TIM1->BDTR = 5U;
    assert(INV_Init() == HAL_OK);
    assert(!INV_PwmFlag && !(TIM1->BDTR & TIM_BDTR_MOE));
    assert((TIM1->BDTR & 255U) == 5U);
    assert(TIM5->ARR == 888U);
    assert(TIM1->CR1 & TIM_CR1_CEN);
    assert(TIM5->DIER & TIM_IT_UPDATE);

    INV_SetFrequency(INT32_MAX);
    assert(INV_Freq == INV_FREQ_MAX && TIM5->ARR == 443U);
    INV_SetFrequency(INT32_MIN);
    assert(INV_Freq == INV_FREQ_MIN && TIM5->ARR == 1480U);
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
    uint32_t edge_margin = INV_PERIOD * (100U - INV_AMP_MAX) / 200U;
    assert(minimum <= edge_margin + 2U && maximum >= INV_PERIOD - edge_margin - 2U);
    float expected_peak = INV_PERIOD * INV_AMP_MAX / 200.0f;
    float expected_square_mean = expected_peak * expected_peak / 2.0f;
    assert(fabsf((float)squares / INV_SAMP_NUMBER - expected_square_mean) <
           expected_square_mean * 0.03f);

    INV_SetAmplitude(INT32_MIN);
    assert(INV_Amp == INV_AMP_MIN);
    HAL_TIM_PeriodElapsedCallback(&htim5);
    uint32_t minimum_amp_counts = INV_PERIOD * INV_AMP_MIN / 200U;
    for (uint32_t ch = 0; ch < 3U; ch++)
        assert(TIM1->compare[ch] >= INV_PERIOD / 2 - minimum_amp_counts &&
               TIM1->compare[ch] <= INV_PERIOD / 2 + minimum_amp_counts);
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

static void CheckVoltageControl(void)
{
    INV_SetAmplitude(50);
    INV_PwmFlag = 1;
    INV_ResetVoltageControl();
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE - 1U; i++)
    {
        assert(INV_UpdateVoltageControl(10.0f, 20.0f, 1.0f, 1.0f, 0.1f) == -1.0f);
        assert(INV_Amp == 50);
    }
    assert(INV_UpdateVoltageControl(10.0f, 20.0f, 1.0f, 1.0f, 0.1f) == 10.0f);
    assert(INV_Amp == 61); /* 满窗才执行比例与积分调节。 */
    assert(INV_UpdateVoltageControl(10.0f, 20.0f, 1.0f, 1.0f, 0.1f) == 10.0f);
    assert(INV_Amp == 61); /* 下一个窗口未满，不得重复调幅。 */
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE - 1U; i++)
        INV_UpdateVoltageControl(10.0f, 20.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == 62);
    INV_SetAmplitude(50);
    INV_ResetVoltageControl();
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE; i++)
        INV_UpdateVoltageControl(0.0f, 10.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == 61); /* RMS 为 0 也是有效结果，应执行 PI。 */
    INV_ResetVoltageControl();
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE - 1U; i++)
        assert(INV_CalculateVoltageRms(10.0f) < 0.0f);
    assert(fabsf(INV_CalculateVoltageRms(-10.0f) - 10.0f) < 0.001f);
    /* 新窗口完全替换旧窗口，不能残留旧样本的能量。 */
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE; i++)
        INV_CalculateVoltageRms(0.0f);
    assert(INV_CalculateVoltageRms(0.0f) == 0.0f);
    INV_ResetVoltageControl();
    float rms = -1.0f;
    for (uint32_t i = 0; i < 3U * INV_RMS_WINDOW_SIZE; i++)
        rms = INV_CalculateVoltageRms(20.0f * sinf(6.28318530718f *
                                      (float)i / INV_RMS_WINDOW_SIZE));
    assert(fabsf(rms - 20.0f / sqrtf(2.0f)) < 0.001f);

    INV_SetAmplitude(50);
    INV_PwmFlag = 1;
    INV_ResetVoltageControl();
    INV_VoltagePI(110.0f, 100.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == 61); /* 50 + Kp*10 + Ki*10*0.1 */
    INV_VoltagePI(100.0f, 100.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == 51); /* 积分保留，比例项归零。 */
    INV_VoltagePI(100.0f, -1.0f, 1.0f, 1.0f, 0.1f);
    INV_VoltagePI(100.0f, 90.0f, 1.0f, 1.0f, 0.0f);
    assert(INV_Amp == 51);
    for (uint32_t i = 0; i < 1000U; i++)
        INV_VoltagePI(200.0f, 0.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == INV_AMP_MAX);
    INV_VoltagePI(100.0f, 100.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == 51); /* 长时间上限饱和未造成积分累积。 */
    for (uint32_t i = 0; i < 1000U; i++)
        INV_VoltagePI(0.0f, 200.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == INV_AMP_MIN);
    INV_VoltagePI(100.0f, 100.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == 51);
    INV_SetAmplitude(50);
    INV_ResetVoltageControl();
    INV_VoltagePI(100.0f, 90.0f, 0.0f, 100.0f, 1.0f);
    assert(INV_Amp == INV_AMP_MAX);
    INV_VoltagePI(100.0f, 100.0f, 0.0f, 100.0f, 1.0f);
    assert(INV_Amp == INV_AMP_MAX); /* 纯积分跨越边界后仍能保持输出。 */
    INV_OutputDisable();
    INV_VoltagePI(200.0f, 0.0f, 1.0f, 1.0f, 0.1f);
    assert(INV_Amp == INV_AMP_MAX);
    assert(INV_CalculateVoltageRms(10.0f) < 0.0f);

    /* INV_VoltageControlTick：逐帧累加平方，满窗才调一次 PI。 */
    INV_SetAmplitude(50);
    INV_SetTargetVoltage(20.0f);
    INV_PwmFlag = 1;
    INV_ResetVoltageControl();
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE; i++)
        INV_VoltageControlTick(10.0f);
    assert(fabsf(INV_GetVoltageRms() - 10.0f) < 0.001f);
    assert(INV_Amp == 74); /* 50 + Kp*(20-10) + Ki*(20-10)*0.02 */

    /* 闭环关闭时仍更新实测值供显示，但不得改幅值。 */
    INV_SetAmplitude(50);
    INV_PwmFlag = 0;
    INV_ResetVoltageControl();
    for (uint32_t i = 0; i < INV_RMS_WINDOW_SIZE; i++)
        INV_VoltageControlTick(5.0f);
    assert(fabsf(INV_GetVoltageRms() - 5.0f) < 0.001f);
    assert(INV_Amp == 50);
    INV_SetTargetVoltage(0.0f);
    INV_ResetVoltageControl();
}

static void FeedSampling(uint16_t v1, uint16_t v2, uint16_t current)
{
    static uint32_t half;
    uint32_t index = half * INV_SAMPLE_CHANNELS;
    sampling_dma[index] = v1;
    sampling_dma[index + 1U] = v2;
    sampling_dma[index + 2U] = current;
    if (half == 0U) HAL_ADC_ConvHalfCpltCallback(&hadc1);
    else HAL_ADC_ConvCpltCallback(&hadc1);
    half ^= 1U;
}

static void CheckSampling(void)
{
    INV_SamplingData data;
    assert(INV_SamplingConvert(100U, 0.5f, -3.0f) == 47.0f);
    test_adc_status = HAL_ERROR;
    assert(INV_SamplingStart() == HAL_ERROR && test_adc_dma_starts == 0U);
    test_adc_status = HAL_OK;
    assert(INV_SamplingStart() == HAL_OK && test_adc_dma_starts == 1U);
    FeedSampling(4000U, 4000U, 4000U); /* 启动时的不完整周期不得用于偏置。 */
    INV_SamplingNotifyCycle();
    FeedSampling(2100U, 1050U, 3020U);
    INV_SamplingGetData(&data);
    assert(!data.offset_valid);
    FeedSampling(2000U, 1000U, 3000U);
    FeedSampling(1900U, 950U, 2980U);
    FeedSampling(2000U, 1000U, 3000U);
    INV_SamplingNotifyCycle();
    FeedSampling(2200U, 1150U, 3120U);
    INV_SamplingGetData(&data);
    assert(data.offset_valid && data.sequence == 6U);
    assert(data.raw[0] == 2200U && data.raw[1] == 1150U && data.raw[2] == 3120U);
    assert(data.value[0] == INV_SamplingConvert(2200U, INV_SAMPLE_V1_K, INV_SAMPLE_V1_B));
    assert(data.offset[0] == INV_SamplingConvert(2000U, INV_SAMPLE_V1_K, INV_SAMPLE_V1_B));
    assert(data.offset[1] == INV_SamplingConvert(1000U, INV_SAMPLE_V2_K, INV_SAMPLE_V2_B));
    assert(data.offset[2] == INV_SamplingConvert(3000U, INV_SAMPLE_I_K, INV_SAMPLE_I_B));
    assert(fabsf(data.ac[0] - 200.0f * INV_SAMPLE_V1_K) < 0.001f);
    assert(fabsf(data.ac[1] - 150.0f * INV_SAMPLE_V2_K) < 0.001f);
    assert(fabsf(data.ac[2] - 120.0f * INV_SAMPLE_I_K) < 0.001f);
    FeedSampling(2100U, 1100U, 3100U);
    FeedSampling(2000U, 1050U, 3080U);
    FeedSampling(2100U, 1100U, 3100U);
    INV_SamplingGetData(&data);
    /* 周期内固定使用上一周的偏置。 */
    assert(data.offset[0] == INV_SamplingConvert(2000U, INV_SAMPLE_V1_K, INV_SAMPLE_V1_B));
    INV_SamplingNotifyCycle();
    FeedSampling(2400U, 1400U, 3400U);
    INV_SamplingGetData(&data);
    assert(data.offset[0] == INV_SamplingConvert(2100U, INV_SAMPLE_V1_K, INV_SAMPLE_V1_B));
    assert(data.offset[1] == INV_SamplingConvert(1100U, INV_SAMPLE_V2_K, INV_SAMPLE_V2_B));
    assert(data.offset[2] == INV_SamplingConvert(3100U, INV_SAMPLE_I_K, INV_SAMPLE_I_B));
    assert(fabsf(data.ac[0] - 300.0f * INV_SAMPLE_V1_K) < 0.001f);
    assert(fabsf(data.ac[1] - 300.0f * INV_SAMPLE_V2_K) < 0.001f);
    assert(fabsf(data.ac[2] - 300.0f * INV_SAMPLE_I_K) < 0.001f);
    test_primask = 1U;
    INV_SamplingGetData(&data);
    assert(test_primask == 1U);
    test_primask = 0U;
    assert(INV_SamplingStop() == HAL_OK);
    INV_SamplingGetData(&data);
    assert(!data.offset_valid);
    assert(INV_SamplingStart() == HAL_OK);
    INV_SamplingGetData(&data);
    assert(!data.offset_valid && data.sequence == 0U);
    INV_SamplingStop();
}

static void CheckMenu(void)
{
    INV_Params params;

    INV_SetFrequency(50);
    INV_SetAmplitude(10);
    INV_SetTargetVoltage(0.0f);
    INV_OutputEnable();
    test_keys = "6826D";
    INV_Menu_Run();
    assert(INV_Freq == 52 && INV_TargetVoltage == 0.0f && INV_Amp == 10 && !INV_PwmFlag);
    test_keys = "86C";
    INV_Menu_Run();
    assert(INV_Freq == 52 && INV_TargetVoltage == 0.5f && INV_Amp == 10 && !INV_PwmFlag);
    INV_GetParams(&params);
    assert(params.frequency == 52 && params.target_voltage == 0.5f &&
           params.amplitude == 10 && params.output_enabled == 0 &&
           params.voltage_rms < 0.0f);
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
    CheckVoltageControl();
    CheckSampling();
    CheckMenu();
    CheckOled();
    puts("inverter_check: PASS (SPWM, frequency, amplitude, output, RMS, PI, ADC sampling/offset, menu, OLED)");
    return 0;
}
