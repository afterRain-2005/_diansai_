#ifndef INVERTER_CONTROL_TOOLS_H
#define INVERTER_CONTROL_TOOLS_H

#include <stdint.h>

#define INV_FREQ_MIN      30
#define INV_FREQ_MAX      100
#define INV_FREQ_DEFAULT  50
#define INV_AMP_MIN       1
#define INV_AMP_MAX       99
#define INV_AMP_DEFAULT   90

/* TIM1 Update 名义触发 20 kHz，400 点对应默认 50 Hz 的一周（20 ms）。
 * 接入闭环前须实测采样率；基波频率改变时，固定窗口不一定覆盖整数周。
 */
#define INV_RMS_WINDOW_SIZE 400U

extern volatile int32_t INV_Freq;    /* 基波频率，Hz */
extern volatile int32_t INV_Amp;     /* 调制幅度，% */
extern volatile int32_t INV_PwmFlag; /* 软件启停命令，不代表硬件输出测量值 */

void INV_SetFrequency(int32_t frequency);
void INV_SetAmplitude(int32_t amplitude);
/* 单组状态，须在同一执行上下文串行调用；开始闭环或改变采样配置时复位。
 * 复位不改变 INV_Amp，关闭输出时也会自动复位。
 */
void INV_ResetVoltageControl(void);
/* 每个新样本调用一次，输入为已换算且去除采样电路偏置的瞬时电压，单位 V。
 * 窗口未满时返回 -1.0f；调用方须保证输入及平方运算结果为有限数值。
 */
float INV_CalculateVoltageRms(float voltage);
/* 位置式 PI，通过 INV_SetAmplitude 写入百分比输出。
 * kp: %/V，ki: %/(V*s)，dt: 两次 PI 调用间隔，单位 s。
 * RMS 或增益为负、dt 不大于 0、输出未启用时不调整幅值。
 * 调用方须保证参数及中间运算结果为有限数值。
 */
void INV_VoltagePI(float target_rms, float measured_rms,
                   float kp, float ki, float dt);
/* 每个新电压样本调用一次：窗口未满时只累计样本，满窗后每次调用执行 PI。
 * 返回当前 RMS，未满窗返回 -1.0f；dt 为采样间隔（s），不包含等待满窗的时间。
 * 开始前先调用 INV_ResetVoltageControl；不要再对同一个样本单独调用 RMS/PI。
 */
float INV_UpdateVoltageControl(float voltage, float target_rms,
                               float kp, float ki, float dt);
void INV_OutputEnable(void);
void INV_OutputDisable(void);

#endif
