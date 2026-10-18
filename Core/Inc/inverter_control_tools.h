#ifndef INVERTER_CONTROL_TOOLS_H
#define INVERTER_CONTROL_TOOLS_H

#include <stdint.h>

#define INV_FREQ_MIN      30
#define INV_FREQ_MAX      100
#define INV_FREQ_DEFAULT  50
#define INV_AMP_MIN       1
#define INV_AMP_MAX       99
#define INV_AMP_DEFAULT   90

/* 默认窗口：TIM1 更新名义触发 20 kHz 时，400 点对应 50 Hz 的一周（20 ms）。
 * 实际窗口由 INV_SetFrequency 按 20000/频率 四舍五入重算（30 Hz 667 点，
 * 100 Hz 200 点），切频时复位闭环等新窗口填满；接入前须实测采样率。
 */
#define INV_RMS_WINDOW_SIZE 400U

extern volatile int32_t INV_Freq;    /* 基波频率，Hz */
extern volatile int32_t INV_Amp;     /* 调制幅度，%；闭环下由 PI 自动写入 */
extern volatile int32_t INV_PwmFlag; /* 软件启停命令，不代表硬件输出测量值 */
extern volatile float INV_TargetVoltage; /* 闭环目标电压（RMS） */

/* 菜单显示用参数快照：控制层在中断里更新，主循环一次读出整组值。 */
typedef struct
{
    int32_t frequency;      /* 目标频率，Hz */
    float target_voltage;   /* 目标有效值，V */
    int32_t amplitude;      /* 当前调制幅度，%；已乘进占空比 */
    float voltage_rms;      /* 实测有效值，V；窗口未满为 -1.0f */
    int32_t output_enabled; /* 软件启停命令 */
} INV_Params;

/* 改频率的同时重算 TIM5 重载值和 RMS 窗口；窗口长度变化时复位闭环状态。 */
void INV_SetFrequency(int32_t frequency);
void INV_SetAmplitude(int32_t amplitude);
void INV_SetTargetVoltage(float target);
void INV_GetParams(INV_Params *params);
/* 最近一个 RMS 窗口的有效值；窗口未满返回 -1.0f。 */
float INV_GetVoltageRms(void);
/* 每个采样帧调用一次：始终更新显示用 RMS，目标大于 0 且已开输出时才做闭环。 */
void INV_VoltageControlTick(float voltage);
/* 单组状态，须在同一执行上下文串行调用；开始闭环或改变采样配置时复位。
 * 复位不改变 INV_Amp，关闭输出时也会自动复位。
 */
void INV_ResetVoltageControl(void);
/* 每个新样本调用一次，输入为已换算且去除采样电路偏置的瞬时电压，单位 V。
 * 窗口未满时返回 -1.0f；调用方须保证输入及平方运算结果为有限数值。
 */
float INV_CalculateVoltageRms(float voltage);
/* 增量式 PI：Δu = Kp*(e-e_prev) + Ki*e*dt，叠在上一次幅值上再调 INV_SetAmplitude。
 * 没有积分累加器，幅值饱和后不会积攒反向退出量；首次调用取 e_prev=0。
 * kp: %/V，ki: %/(V*s)，dt: 两次 PI 调用间隔，单位 s。
 * RMS 或增益为负、dt 不大于 0、输出未启用时不调整幅值。
 * 调用方须保证参数及中间运算结果为有限数值。
 */
void INV_VoltagePI(float target_rms, float measured_rms,
                   float kp, float ki, float dt);
/* 每个新电压样本调用一次：窗口未满时只累计样本，之后每满一个窗口执行一次 PI。
 * 返回当前 RMS，未满窗返回 -1.0f；窗口长度随频率变化，dt 传一个窗口时长（1/频率）。
 * 开始前先调用 INV_ResetVoltageControl；不要再对同一个样本单独调用 RMS/PI。
 */
float INV_UpdateVoltageControl(float voltage, float target_rms,
                               float kp, float ki, float dt);
void INV_OutputEnable(void);
void INV_OutputDisable(void);

#endif
