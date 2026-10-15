#ifndef __BIPOLAR_PWM_H
#define __BIPOLAR_PWM_H

#include "main.h"
#include <stdint.h>

#define INV_TIM_CLK              170000000UL
#define INV_CARRIER_HZ           50000UL
#define INV_PERIOD              (INV_TIM_CLK / INV_CARRIER_HZ)
#define INV_SAMP_NUMBER          360
#define INV_DEAD_TIME_NS         1000
#define INV_DEAD_TIME_DTG        0x95U
#define INV_MIN_GATE_PULSE_NS    200U
#define INV_SOFT_START_MS        500U
#define INV_ADC_CHANNELS         4U
#define INV_ADC_HALF_FRAMES      16U
#define INV_ADC_TIMEOUT_MS       10U
#define INV_CONTROL_TIMEOUT_MS   100U
#define INV_DC_BUS_NOMINAL_V     48.0f
#define INV_DC_BUS_MIN_V         40.0f
#define INV_DC_BUS_MAX_V         60.0f
#define INV_OUTPUT_RMS_TARGET_V  24.0f
#define INV_OUTPUT_MAX_RMS_V     30.0f
#define INV_VOLTAGE_SENSOR_GAIN  10.0f
#define INV_VOLTAGE_SENSOR_OFFSET_V 0.0f
#define INV_CURRENT_SENSOR_GAIN  0.1f
#define INV_CURRENT_SENSOR_OFFSET_V 1.65f
#define INV_OUTPUT_FILTER_L_H    0.001f
#define INV_OUTPUT_FILTER_C_F    0.000001f
#define INV_OUTPUT_LOAD_MAX_OHM  10.0f
#define INV_FREQ_MIN             30
#define INV_FREQ_MAX             80
#define INV_AMP_MIN              0
#define INV_AMP_MAX              91
#define INV_FREQ_DEFAULT         50
#define INV_AMP_DEFAULT          10
#define INV_RMS_REMOVE_DC        1
#define INV_VOLTAGE_GAIN         INV_VOLTAGE_SENSOR_GAIN
#define INV_ADC_REFERENCE_V      3.3f
#define INV_VOLTAGE_CALIBRATED    0

#define INV_CLOSED_LOOP_ENABLE          1
#define INV_CLOSED_LOOP_ADC_CHANNEL     0U
#define INV_CLOSED_LOOP_TARGET_ADC_RMS  1000U
#define INV_CLOSED_LOOP_KP              1000.0f
#define INV_CLOSED_LOOP_KI              1.0f
#define INV_CLOSED_LOOP_MAX_AMP         88

#define INV_FAULT_BREAK          0x01U
#define INV_FAULT_ADC            0x02U
#define INV_FAULT_TIMEOUT        0x04U
#define INV_FAULT_INIT           0x08U
#define INV_FAULT_CPU            0x10U

extern volatile uint16_t INV_ADC_Results[INV_ADC_CHANNELS];
extern volatile int32_t INV_Freq;
extern volatile int32_t INV_Amp;
extern volatile int32_t INV_Vrms;
extern volatile int32_t INV_AdcVrms;
extern volatile int32_t INV_AdcCh[INV_ADC_CHANNELS];
extern volatile int32_t INV_PwmFlag;
extern volatile int32_t INV_BreakFlag;
extern volatile uint32_t INV_FaultFlags;

void sin_duty_init(uint16_t timer_period, int32_t samp_num);
void Bipolar_pwm(uint16_t pwm_period, int32_t samp_num);
void INV_SetFrequency(int32_t freq);
void INV_SetAmplitude(int32_t amplitude);
int32_t INV_GetMaxAmplitude(void);
HAL_StatusTypeDef INV_SetDeadTime(int32_t deadtime_ns);
HAL_StatusTypeDef INV_OutputEnable(void);
void INV_OutputDisable(void);
HAL_StatusTypeDef INV_ClearFault(void);
HAL_StatusTypeDef INV_Init(void);
void INV_EmergencyStop(void);
void INV_SafetyTick(void);
void INV_Service(void);
HAL_StatusTypeDef INV_SamplingPause(void);
HAL_StatusTypeDef INV_SamplingResume(void);
int32_t PI_Calculate(uint16_t cur_value, uint16_t set_value, float kp, float ki);
void Vout_Ctr(uint16_t cur_v, uint16_t set_v, float kp, float ki);

#endif
