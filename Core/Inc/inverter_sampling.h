#ifndef INVERTER_SAMPLING_H
#define INVERTER_SAMPLING_H

#include "main.h"

enum
{
    INV_SAMPLE_VOLTAGE1 = 0, /* ADC1 Rank1 / IN6 / PC0 */
    INV_SAMPLE_VOLTAGE2,     /* ADC1 Rank2 / IN7 / PC1 */
    INV_SAMPLE_CURRENT,      /* ADC1 Rank3 / IN8 / PC2 */
    INV_SAMPLE_CHANNELS
};

/* 一次线性拟合：实际量 = K * ADC原始计数 + B。
 * 占位值尚未标定，换算结果仍是 ADC 计数单位。
 */
#define INV_SAMPLE_V1_K 0.0694317f
#define INV_SAMPLE_V1_B 1.660f
#define INV_SAMPLE_V2_K 0.0672282f
#define INV_SAMPLE_V2_B 1.667f
#define INV_SAMPLE_I_K  0.00263259f
#define INV_SAMPLE_I_B  1.660f

typedef struct
{
    uint16_t raw[INV_SAMPLE_CHANNELS];
    float value[INV_SAMPLE_CHANNELS];  /* K*raw+B，包含采样偏置 */
    float offset[INV_SAMPLE_CHANNELS]; /* 上一完整周期的均值，已换算 */
    float ac[INV_SAMPLE_CHANNELS];     /* value-offset */
    uint32_t offset_valid;             /* 为 1 时 ac 才可用于 RMS/PI */
    uint32_t sequence;                 /* 已处理的三通道采样帧数 */
} INV_SamplingData;

/* 外设初始化完成、TIM1 启动前调用；校准 ADC 并启动三路循环 DMA。 */
HAL_StatusTypeDef INV_SamplingStart(void);
HAL_StatusTypeDef INV_SamplingStop(void);
/* 主循环读取一致快照；不会触发采样或 PI。 */
void INV_SamplingGetData(INV_SamplingData *data);
float INV_SamplingConvert(uint16_t raw, float k, float b);
/* 由正弦表索引到 0 时通知，仅递增周期号。 */
void INV_SamplingNotifyCycle(void);
/* 仅在 DMA 数据处理入口调用，以上一完整周期均值更新本周偏置。
 * 启动后的不完整周期丢弃；之后收满一周才置 offset_valid。
 */
void INV_SamplingUpdateOffset(void);

#endif /* INVERTER_SAMPLING_H */
