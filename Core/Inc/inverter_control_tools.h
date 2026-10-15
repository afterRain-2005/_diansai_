#ifndef INVERTER_CONTROL_TOOLS_H
#define INVERTER_CONTROL_TOOLS_H

#include <stdint.h>

#define INV_FREQ_MIN      30
#define INV_FREQ_MAX      80
#define INV_FREQ_DEFAULT  50
#define INV_AMP_MIN       0
#define INV_AMP_MAX       100
#define INV_AMP_DEFAULT   10

extern volatile int32_t INV_Freq;    /* 基波频率，Hz */
extern volatile int32_t INV_Amp;     /* 调制幅度，% */
extern volatile int32_t INV_PwmFlag; /* 软件启停命令，不代表硬件输出测量值 */

void INV_SetFrequency(int32_t frequency);
void INV_SetAmplitude(int32_t amplitude);
void INV_OutputEnable(void);
void INV_OutputDisable(void);

#endif
