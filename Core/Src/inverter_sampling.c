#include "inverter_sampling.h"

extern ADC_HandleTypeDef hadc1;

/* 两帧，每帧依次为 Rank1、Rank2、Rank3；半满/全满回调各处理一帧。 */
__ALIGNED(4) static volatile uint16_t sampling_dma[2U * INV_SAMPLE_CHANNELS];
static volatile INV_SamplingData sampling_data;
static const float sampling_k[INV_SAMPLE_CHANNELS] =
    {INV_SAMPLE_V1_K, INV_SAMPLE_V2_K, INV_SAMPLE_I_K};
static const float sampling_b[INV_SAMPLE_CHANNELS] =
    {INV_SAMPLE_V1_B, INV_SAMPLE_V2_B, INV_SAMPLE_I_B};
static volatile uint32_t sampling_cycle;
static uint32_t previous_cycle, cycle_synced, cycle_count;
static uint32_t cycle_sum[INV_SAMPLE_CHANNELS];
static float raw_offset[INV_SAMPLE_CHANNELS];

float INV_SamplingConvert(uint16_t raw, float k, float b)
{
    return k * raw + b;
}

void INV_SamplingNotifyCycle(void)
{
    sampling_cycle++;
}

void INV_SamplingUpdateOffset(void)
{
    uint32_t cycle = sampling_cycle;

    if (cycle_synced && cycle_count != 0U)
    {
        for (uint32_t ch = 0; ch < INV_SAMPLE_CHANNELS; ch++)
            raw_offset[ch] = (float)cycle_sum[ch] / cycle_count;
        sampling_data.offset_valid = 1U;
    }
    else
        sampling_data.offset_valid = 0U;

    for (uint32_t ch = 0; ch < INV_SAMPLE_CHANNELS; ch++)
        cycle_sum[ch] = 0U;
    cycle_count = 0U;
    previous_cycle = cycle;
    cycle_synced = 1U;
}

static void SamplingProcessFrame(const volatile uint16_t *raw)
{
    /* 周期切换与累加均只在 DMA 回调中执行，TIM5 不读写这些状态。 */
    INV_SamplingUpdateOffset();
    for (uint32_t ch = 0; ch < INV_SAMPLE_CHANNELS; ch++)
    {
        uint16_t sample = raw[ch];
        sampling_data.raw[ch] = sample;
        sampling_data.value[ch] = INV_SamplingConvert(sample, sampling_k[ch], sampling_b[ch]);
        sampling_data.offset[ch] = sampling_k[ch] * raw_offset[ch] + sampling_b[ch];
        sampling_data.ac[ch] = sampling_data.value[ch] - sampling_data.offset[ch];
        cycle_sum[ch] += sample;
    }
    cycle_count++;
    sampling_data.sequence++;
}

HAL_StatusTypeDef INV_SamplingStart(void)
{
    /* 仅在采样停止时调用，避免与 DMA 数据处理并发复位。 */
    sampling_data = (INV_SamplingData){0};
    previous_cycle = sampling_cycle;
    cycle_synced = 0U;
    cycle_count = 0U;
    for (uint32_t ch = 0; ch < INV_SAMPLE_CHANNELS; ch++)
    {
        cycle_sum[ch] = 0U;
        raw_offset[ch] = 0.0f;
    }
    HAL_StatusTypeDef status = HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    if (status != HAL_OK) return status;
    return HAL_ADC_Start_DMA(&hadc1, (uint32_t *)sampling_dma, 2U * INV_SAMPLE_CHANNELS);
}

HAL_StatusTypeDef INV_SamplingStop(void)
{
    HAL_StatusTypeDef status = HAL_ADC_Stop_DMA(&hadc1);
    if (status == HAL_OK) sampling_data.offset_valid = 0U;
    return status;
}

void INV_SamplingGetData(INV_SamplingData *data)
{
    if (data == NULL) return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *data = sampling_data;
    __set_PRIMASK(primask);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1) SamplingProcessFrame(sampling_dma);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == &hadc1) SamplingProcessFrame(&sampling_dma[INV_SAMPLE_CHANNELS]);
}
