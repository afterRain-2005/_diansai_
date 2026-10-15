#include "Bipolar_PWM.h"
#include "inverter_math.h"
#include "inverter_control_tools.h"
#include "Keyboard.h"
#include <limits.h>

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim5;
extern ADC_HandleTypeDef hadc1;

volatile uint16_t INV_ADC_Results[INV_ADC_CHANNELS];
volatile int32_t INV_Freq = INV_FREQ_DEFAULT;
volatile int32_t INV_Amp = INV_AMP_DEFAULT;
volatile int32_t INV_Vrms;
volatile int32_t INV_AdcVrms;
volatile int32_t INV_AdcCh[INV_ADC_CHANNELS];
volatile int32_t INV_PwmFlag;
volatile int32_t INV_BreakFlag;
volatile uint32_t INV_FaultFlags;

static int16_t sin_data[2 * INV_SAMP_NUMBER];
static uint16_t adc_dma[2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS] __ALIGNED(4);
static int32_t table_samples;
static uint16_t table_period;
static uint32_t phase_index;
static uint32_t active_amplitude;
static uint32_t minimum_compare;
static uint64_t rms_sum;
static uint64_t rms_squares;
static uint32_t rms_count;
static uint32_t rms_window;
static float pi_integral;
static float pi_fraction;
static volatile uint32_t initialized;
static volatile uint32_t sampling_paused;
static volatile uint32_t adc_ready;
static volatile uint32_t adc_tick;
static volatile uint32_t control_tick;
static volatile uint32_t modulation_tick;
static volatile uint32_t rms_sequence;
static uint32_t control_rms_sequence;

static void ResetRms(void)
{
    rms_sum = 0U;
    rms_squares = 0U;
    rms_count = 0U;
    rms_window = (INV_CARRIER_HZ + (uint32_t)INV_Freq / 2U) / (uint32_t)INV_Freq;
}

static void LatchFault(uint32_t fault)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    INV_OutputDisable();
    INV_FaultFlags |= fault;
    if ((fault & INV_FAULT_BREAK) != 0U)
    {
        INV_BreakFlag = 1;
        __HAL_TIM_DISABLE_IT(&htim1, TIM_IT_BREAK);
    }
    __set_PRIMASK(primask);
}

void INV_OutputDisable(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U)
        CLEAR_BIT(TIM1->BDTR, TIM_BDTR_MOE);
    if ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN) != 0U)
        GPIOA->BSRR = ((uint32_t)PWM_EN_Pin << 16U) | INV_OFFLINE_Pin;
    if ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOBEN) != 0U)
        GPIOB->BSRR = (uint32_t)RS_CTRL_Pin << 16U;
    INV_PwmFlag = 0;
    active_amplitude = 0U;
    pi_integral = 0.0f;
    pi_fraction = 0.0f;
    control_rms_sequence = rms_sequence;
    __set_PRIMASK(primask);
}

void INV_EmergencyStop(void)
{
    LatchFault(INV_FAULT_CPU);
}

int32_t INV_GetMaxAmplitude(void)
{
    int32_t maximum;

    if (minimum_compare * 2U >= INV_PERIOD) return 0;
    maximum = (int32_t)((INV_PERIOD - 2U * minimum_compare) * 100U / INV_PERIOD);
    return maximum < INV_AMP_MAX ? maximum : INV_AMP_MAX;
}

void INV_SetAmplitude(int32_t amplitude)
{
    int32_t maximum = INV_GetMaxAmplitude();

    if (amplitude < INV_AMP_MIN) amplitude = INV_AMP_MIN;
    if (amplitude > maximum) amplitude = maximum;
    INV_Amp = amplitude;
}

void sin_duty_init(uint16_t timer_period, int32_t samp_num)
{
    int32_t sample_index;

    if (INV_PwmFlag || timer_period != INV_PERIOD || samp_num != INV_SAMP_NUMBER)
    {
        LatchFault(INV_FAULT_INIT);
        return;
    }
    for (sample_index = 0; sample_index < samp_num; sample_index++)
        sin_data[sample_index] = (int16_t)((float)timer_period * 0.5f *
            sinf(6.28318530718f * (float)sample_index / (float)samp_num));
    for (sample_index = samp_num; sample_index < 2 * INV_SAMP_NUMBER; sample_index++)
        sin_data[sample_index] = sin_data[sample_index - samp_num];
    table_samples = samp_num;
    table_period = timer_period;
    phase_index = 0U;
}

static uint32_t DutyAt(uint32_t index, uint32_t period)
{
    int32_t duty = (int32_t)period / 2 +
        (int32_t)sin_data[index] * (int32_t)active_amplitude / 100000;

    if (duty < (int32_t)minimum_compare) duty = (int32_t)minimum_compare;
    if (duty > (int32_t)(period - minimum_compare)) duty = (int32_t)(period - minimum_compare);
    return (uint32_t)duty;
}

void Bipolar_pwm(uint16_t pwm_period, int32_t samp_num)
{
    uint32_t duty_a, duty_b, duty_c;
    uint32_t update_disabled;
    uint32_t primask;

    if (pwm_period != table_period || samp_num != table_samples || samp_num < 3)
    {
        LatchFault(INV_FAULT_INIT);
        return;
    }
    duty_a = DutyAt(phase_index, pwm_period);
    duty_b = DutyAt(phase_index + (uint32_t)samp_num / 3U, pwm_period);
    duty_c = DutyAt(phase_index + (uint32_t)samp_num * 2U / 3U, pwm_period);
    primask = __get_PRIMASK();
    __disable_irq();
    update_disabled = TIM1->CR1 & TIM_CR1_UDIS;
    SET_BIT(TIM1->CR1, TIM_CR1_UDIS);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_a);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty_b);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty_c);
    if (update_disabled == 0U) CLEAR_BIT(TIM1->CR1, TIM_CR1_UDIS);
    __set_PRIMASK(primask);
    phase_index++;
    if (phase_index >= (uint32_t)samp_num) phase_index = 0U;
}

void INV_SetFrequency(int32_t freq)
{
    uint32_t primask = __get_PRIMASK();
    uint32_t rate;

    if (freq < INV_FREQ_MIN) freq = INV_FREQ_MIN;
    if (freq > INV_FREQ_MAX) freq = INV_FREQ_MAX;
    rate = (uint32_t)INV_SAMP_NUMBER * (uint32_t)freq;
    __disable_irq();
    INV_Freq = freq;
    if (htim5.Instance != NULL)
        __HAL_TIM_SET_AUTORELOAD(&htim5, (INV_TIM_CLK + rate / 2U) / rate - 1U);
    ResetRms();
    __set_PRIMASK(primask);
}

HAL_StatusTypeDef INV_SetDeadTime(int32_t deadtime_ns)
{
    uint32_t code;
    uint32_t margin;
    uint32_t primask = __get_PRIMASK();

    if (!INV_EncodeDeadTime(deadtime_ns, INV_TIM_CLK, &code)) return HAL_ERROR;
    margin = INV_DeadTimeTicks(code) +
        (INV_MIN_GATE_PULSE_NS * (INV_TIM_CLK / 1000000U) + 999U) / 1000U;
    if (margin * 2U >= INV_PERIOD || htim1.Instance == NULL) return HAL_ERROR;
    __disable_irq();
    if (INV_PwmFlag || (TIM1->BDTR & TIM_BDTR_MOE) != 0U)
    {
        __set_PRIMASK(primask);
        return HAL_BUSY;
    }
    MODIFY_REG(TIM1->BDTR, TIM_BDTR_DTG, code);
    minimum_compare = margin;
    INV_SetAmplitude(INV_Amp);
    __set_PRIMASK(primask);
    return HAL_OK;
}

HAL_StatusTypeDef INV_ClearFault(void)
{
    uint32_t primask = __get_PRIMASK();
    HAL_StatusTypeDef status = HAL_ERROR;

    __disable_irq();
    if (initialized && !INV_PwmFlag && adc_ready && !sampling_paused &&
        HAL_GetTick() - adc_tick <= INV_ADC_TIMEOUT_MS &&
        HAL_GetTick() - modulation_tick <= INV_ADC_TIMEOUT_MS &&
        (TIM1->CR1 & TIM_CR1_CEN) != 0U &&
        (TIM5->CR1 & TIM_CR1_CEN) != 0U && (TIM5->DIER & TIM_IT_UPDATE) != 0U &&
        HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_SET &&
        (INV_FaultFlags & ~(INV_FAULT_BREAK | INV_FAULT_TIMEOUT)) == 0U)
    {
        __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
        INV_FaultFlags = 0U;
        INV_BreakFlag = 0;
        __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);
        status = HAL_OK;
    }
    __set_PRIMASK(primask);
    return status;
}

HAL_StatusTypeDef INV_OutputEnable(void)
{
    uint32_t primask = __get_PRIMASK();
    HAL_StatusTypeDef status = HAL_ERROR;

    __disable_irq();
    if (initialized && !sampling_paused && adc_ready && INV_FaultFlags == 0U &&
        HAL_GetTick() - adc_tick <= INV_ADC_TIMEOUT_MS &&
        HAL_GetTick() - modulation_tick <= INV_ADC_TIMEOUT_MS &&
        (TIM1->CR1 & TIM_CR1_CEN) != 0U &&
        (TIM5->CR1 & TIM_CR1_CEN) != 0U && (TIM5->DIER & TIM_IT_UPDATE) != 0U &&
        HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_SET &&
        __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK) == RESET)
    {
        active_amplitude = 0U;
        pi_integral = 0.0f;
        pi_fraction = 0.0f;
        phase_index = 0U;
        Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
        TIM1->EGR = TIM_EGR_UG;
        control_tick = HAL_GetTick();
        modulation_tick = control_tick;
        __HAL_TIM_MOE_ENABLE(&htim1);
        if ((TIM1->BDTR & TIM_BDTR_MOE) != 0U &&
            __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK) == RESET)
        {
            HAL_GPIO_WritePin(INV_OFFLINE_GPIO_Port, INV_OFFLINE_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RS_CTRL_GPIO_Port, RS_CTRL_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PWM_EN_GPIO_Port, PWM_EN_Pin, GPIO_PIN_SET);
            INV_PwmFlag = 1;
            status = HAL_OK;
        }
        else LatchFault(INV_FAULT_BREAK);
    }
    __set_PRIMASK(primask);
    return status;
}

HAL_StatusTypeDef INV_SamplingPause(void)
{
    HAL_StatusTypeDef status;

    INV_OutputDisable();
    sampling_paused = 1U;
    adc_ready = 0U;
    __HAL_TIM_DISABLE_IT(&htim5, TIM_IT_UPDATE);
    status = HAL_ADC_Stop_DMA(&hadc1);
    if (status != HAL_OK) LatchFault(INV_FAULT_ADC);
    return status;
}

HAL_StatusTypeDef INV_SamplingResume(void)
{
    HAL_StatusTypeDef status;

    ResetRms();
    adc_ready = 0U;
    adc_tick = HAL_GetTick();
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma,
                              2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS);
    if (status != HAL_OK)
    {
        LatchFault(INV_FAULT_ADC);
        return status;
    }
    __HAL_TIM_CLEAR_FLAG(&htim5, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE);
    sampling_paused = 0U;
    return HAL_OK;
}

HAL_StatusTypeDef INV_Init(void)
{
    INV_OutputDisable();
    initialized = 0U;
    if (INV_FaultFlags != 0U || htim1.Instance != TIM1 || htim5.Instance != TIM5 ||
        hadc1.Instance != ADC1 || SystemCoreClock != INV_TIM_CLK ||
        TIM1->PSC != 0U || TIM1->ARR != INV_PERIOD - 1U ||
        (TIM1->CR1 & (TIM_CR1_CKD | TIM_CR1_CMS | TIM_CR1_DIR)) != 0U) goto init_error;
    sin_duty_init(INV_PERIOD, INV_SAMP_NUMBER);
    if (INV_SetDeadTime(INV_DEAD_TIME_NS) != HAL_OK) goto init_error;
    INV_SetFrequency(INV_Freq);
    Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
    TIM1->EGR = TIM_EGR_UG;
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) goto init_error;
    SET_BIT(TIM1->CCER, TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E |
                       TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE);
    if (HAL_TIM_Base_Start(&htim1) != HAL_OK) goto init_error;
    if (INV_SamplingResume() != HAL_OK) goto init_error;
    if (HAL_TIM_Base_Start_IT(&htim5) != HAL_OK) goto init_error;
    initialized = 1U;
    control_tick = HAL_GetTick();
    modulation_tick = control_tick;
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
    if (HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_RESET)
        LatchFault(INV_FAULT_BREAK);
    else __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);
    INV_OutputDisable();
    return HAL_OK;

init_error:
    LatchFault(INV_FAULT_INIT);
    return HAL_ERROR;
}

void INV_Service(void)
{
    control_tick = HAL_GetTick();
}

void INV_SafetyTick(void)
{
    uint32_t now = HAL_GetTick();

    if (Keyboard_TakeStopRequest()) INV_OutputDisable();
    if (!initialized || !INV_PwmFlag) return;
    if (HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_RESET ||
        __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK) != RESET ||
        (TIM1->BDTR & TIM_BDTR_MOE) == 0U) LatchFault(INV_FAULT_BREAK);
    else if (now - adc_tick > INV_ADC_TIMEOUT_MS ||
             now - modulation_tick > INV_ADC_TIMEOUT_MS ||
             now - control_tick > INV_CONTROL_TIMEOUT_MS) LatchFault(INV_FAULT_TIMEOUT);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5)
    {
        if (INV_Freq < INV_FREQ_MIN || INV_Freq > INV_FREQ_MAX)
        {
            LatchFault(INV_FAULT_INIT);
            return;
        }
        INV_SetAmplitude(INV_Amp);
        uint32_t target = (uint32_t)INV_Amp * 1000U;
        uint32_t step = 100000000U / ((uint32_t)INV_SAMP_NUMBER *
            (uint32_t)INV_Freq * INV_SOFT_START_MS) + 1U;

        modulation_tick = HAL_GetTick();
        if (!INV_PwmFlag) active_amplitude = 0U;
        else if (active_amplitude > target) active_amplitude = target;
        else if (target - active_amplitude > step) active_amplitude += step;
        else active_amplitude = target;
        Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
#if INV_CLOSED_LOOP_ENABLE
        if (INV_CLOSED_LOOP_ADC_CHANNEL == 0U &&
            rms_sequence != control_rms_sequence && INV_AdcVrms <= 4095)
        {
            control_rms_sequence = rms_sequence;
            Vout_Ctr((uint16_t)INV_AdcVrms,
                     INV_CLOSED_LOOP_TARGET_ADC_RMS,
                     INV_CLOSED_LOOP_KP,
                     INV_CLOSED_LOOP_KI);
        }
#endif
    }
}

static void ProcessAdcBlock(uint32_t start)
{
    uint32_t frame_index, channel_index;

    for (frame_index = 0U; frame_index < INV_ADC_HALF_FRAMES; frame_index++)
    {
        uint32_t offset = start + frame_index * INV_ADC_CHANNELS;
        uint32_t sample = adc_dma[offset];

        for (channel_index = 0U; channel_index < INV_ADC_CHANNELS; channel_index++)
        {
            INV_ADC_Results[channel_index] = adc_dma[offset + channel_index];
            INV_AdcCh[channel_index] = adc_dma[offset + channel_index];
        }
        rms_sum += sample;
        rms_squares += (uint64_t)sample * sample;
        rms_count++;
        if (rms_count >= rms_window)
        {
            float rms = INV_RmsFromSums(rms_squares, rms_sum, rms_count, INV_RMS_REMOVE_DC);
            INV_AdcVrms = (int32_t)(rms + 0.5f);
            INV_Vrms = (int32_t)(rms * INV_ADC_REFERENCE_V * INV_VOLTAGE_GAIN * 10.0f / 4095.0f + 0.5f);
            rms_sequence++;
            ResetRms();
        }
    }
    adc_tick = HAL_GetTick();
    adc_ready = 1U;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1 && !sampling_paused) ProcessAdcBlock(0U);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1 && !sampling_paused)
        ProcessAdcBlock(INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        __HAL_ADC_DISABLE_IT(hadc, ADC_IT_OVR);
        LatchFault(INV_FAULT_ADC);
    }
}

void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) LatchFault(INV_FAULT_BREAK);
}

static float PiStep(uint16_t current, uint16_t target, float kp, float ki)
{
    float error = (float)target - current;
    float output;

    if (!isfinite(kp) || !isfinite(ki) || kp < 0.0f || ki < 0.0f) return 0.0f;
    pi_integral += error;
    if (pi_integral > 3000.0f) pi_integral = 3000.0f;
    if (pi_integral < -3000.0f) pi_integral = -3000.0f;
    output = (kp * error + ki * pi_integral) / 1000.0f;
    if (!isfinite(output)) return 0.0f;
    return output;
}

int32_t PI_Calculate(uint16_t cur_value, uint16_t set_value, float kp, float ki)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    float output = PiStep(cur_value, set_value, kp, ki);
    __set_PRIMASK(primask);

    if (output >= 2147483520.0f) return INT32_MAX;
    if (output <= -2147483648.0f) return INT32_MIN;
    return (int32_t)output;
}

void Vout_Ctr(uint16_t cur_v, uint16_t set_v, float kp, float ki)
{
    float change;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (!INV_PwmFlag || INV_FaultFlags != 0U)
    {
        __set_PRIMASK(primask);
        return;
    }
    change = PiStep(cur_v, set_v, kp, ki);
    if (change > 1.0f) change = 1.0f;
    if (change < -1.0f) change = -1.0f;
    pi_fraction += change;
    if (pi_fraction >= 1.0f || pi_fraction <= -1.0f)
    {
        int32_t step = (int32_t)pi_fraction;
        {
            int32_t next_amplitude = INV_Amp + step;
            if (next_amplitude > INV_CLOSED_LOOP_MAX_AMP)
                next_amplitude = INV_CLOSED_LOOP_MAX_AMP;
            INV_SetAmplitude(next_amplitude);
        }
        pi_fraction -= (float)step;
    }
    __set_PRIMASK(primask);
}
