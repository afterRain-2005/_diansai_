/*
 * Control_Tools.h
 *
 *  Created on: Jul 23, 2025
 *      Author: HuaWenxi
 */
#include <stdbool.h>
#include "stdio.h"
#include "stdint.h"
#ifndef CONTROL_TOOLS_H_
#define CONTROL_TOOLS_H_
//---------------------------------位置式&增量式PI-------------------------------//
typedef struct PI_Controller
{
    float Kp;
    float Ki;
    float Error;
    float Error_last;
    //位置式PI
    float Pos;
    float integral;
    float integral_min;
    float integral_max;
	//增量式PI
	float Pi_Delta;
    float Delta_Max;
    float Delta_Min;
} PI_Controller;
//---------------------------------PR谐振控制器-------------------------------//
typedef struct {
	float output;
    float u_prev1, u_prev2;
    float y_prev1, y_prev2;
    float b0, b1, b2, a1, a2;
    float kp,kr,T,Wc,W0;
    float out_limit;
} PR_Controller;
//---------------------------------交流状态-------------------------------//
typedef struct
{
	float derta_VAC;
	uint8_t AC_State;
} AC_State;
//----------------------------------锁相环-------------------------------//
typedef struct {
    // 输入
	float alpha;
    float beta;
    // 输出
    float ud;
    float uq;
    float sin_theta;
    float cos_theta;
    // 中心频率
    float ws;
    float ws_cen;
    // PI
    PI_Controller pi;
    // 采样时间
    float ts;
} PLL_Controller;
//----------------------------------sogi变换-------------------------------//
#define SIGO_U_GAIN 1
#define SIGO_QU_GAIN 1
//-------下面是sogi的传参，可以用我写的matlab代码SogiCalculate.m来算
// Alpha Channel (D(z)) Coefficients for Fs = 20000 Hz
#define SIGO_U_B0   0.010982881827120f
#define SIGO_U_B1   0.000000000000000f
#define SIGO_U_B2   -0.010982881827120f
#define SIGO_U_A1   -1.977790221205283f
#define SIGO_U_A2   0.978034236345759f
// Beta Channel (Q(z)) Coefficients for Fs = 20000 Hz
#define SIGO_QU_B0   0.000086259352158f
#define SIGO_QU_B1   0.000172518704317f
#define SIGO_QU_B2   0.000086259352158f
#define SIGO_QU_A1   -1.977790221205283f
#define SIGO_QU_A2   0.978034236345759f
typedef struct DIS_2ORDER_TF_COEF_TAG
{//双线性变换
    float gain;
    float B0;
    float B1;
    float B2;
    float A1;
    float A2;
} DIS_2ORDER_TF_COEF_DEF;
typedef struct DIS_2ORDER_TF_DATA_TAG
{//双线性变换
    float output;
    float w0;
    float w1;
    float w2;
} DIS_2ORDER_TF_DATA_DEF;
typedef struct SOGI_PLL_DATA_STRUCT_TAG
{// SOGI结构体
    float Ualpha;
    float Ubeta;
    float freq;

    DIS_2ORDER_TF_COEF_DEF sogi_u_coeff;
    DIS_2ORDER_TF_DATA_DEF sogi_u_data;

    DIS_2ORDER_TF_COEF_DEF sogi_qu_coeff;
    DIS_2ORDER_TF_DATA_DEF sogi_qu_data;

} SOGI_PLL_DATA_DEF;
//----------------------------------MPPT-------------------------------//
typedef struct {
    float  Ipv;
    float  Vpv;
    float  DeltaPmin;
    float  MaxVolt;
    float  MinVolt;
    float  Stepsize;
    float  VmppOut;
    float  DeltaP;
    float LastDeltaP;
    float  PanelPower;
    float  PanelPower_Prev;
    uint8_t mppt_enable;
    uint8_t mppt_first;
} MPPT_PNO_F;//增量式MPPT
typedef struct {
    float  Ipv;
    float  Vpv;
    float  IpvH;
    float  IpvL;
    float  VpvH;
    float  VpvL;
    float  MaxVolt;
    float  MinVolt;
    float  Stepsize;
    float  VmppOut;
    // internal variables
    float  Cond;
    float  IncCond;
    float  IncCond2;
    float  DeltaV;
    float  DeltaI;
    float  VpvOld;
    float  IpvOld;
    float  StepFirst;
    uint8_t mppt_enable;
    uint8_t mppt_first;
} MPPT_INCC_F;//incc式MPPT
//---------------------增量式PI------------------------//
void PI_Delta_Init(PI_Controller *Pi, float min, float max, float kp, float ki);
void PI_Delta_Cal(PI_Controller *Pi, float Pi_in);
//---------------------位置式PI------------------------//
void PI_Pos_Init(PI_Controller *Pi, float min, float max, float kp, float ki);
void PI_Pos_Cal(PI_Controller *Pi, float error);
//---------------------PR函数------------------------//
void PR_Init(PR_Controller* ctrl, float T, float Kp, float Kr,
		float Wc, float W0, float out_limit);
void PR_Cal(PR_Controller* ctrl, float input, float gain) ;
//---------------------Clark变换函数------------------------//
void Clarke_F(float Ua, float Ub, float Uc, float *Ualpha, float *Ubeta);
void inv_Clarke_F(float Udc, float Ualpha, float Ubeta, float *w1, float *w2, float *w3);
//----------------------交流状态检测------------------------//
void AC_State_init(AC_State *AC_Discern_Pamer);
void AC_State_Detect(AC_State *AC_Discern_Pamer, float Vac_PLL);
//----------------------锁相环函数------------------------//
void PLL_Init(PLL_Controller* pll, float kp, float ki, float sample_freq, float nominal_freq);
void PLL_Update(PLL_Controller* pll, float alpha, float beta);
//----------------------Sogi函数------------------------//
void sogi_pll_init(SOGI_PLL_DATA_DEF *spll_obj, float grid_freq);
float discrete_2order_tf(const float input, DIS_2ORDER_TF_COEF_DEF *coeff, DIS_2ORDER_TF_DATA_DEF *data);
void spll_sogi_func(SOGI_PLL_DATA_DEF *spll_obj, float grid_volt_sen);
//----------------------MPPT函数------------------------//
void MPPTDeltaInit(MPPT_PNO_F *v, float Ipv, float Vpv, float DeltaPmin, float MaxVolt, float MinVolt,float Stepsize, float PanelPower_Prev);
void MPPT_PNO_F_FUNC(MPPT_PNO_F *v);
void MPPT_INCC_F_FUNC(MPPT_INCC_F *v);
void MPPT_INCC_F_init(MPPT_INCC_F *v,float Ipv,float Vpv,float MaxVolt,float MinVolt,float Stepsize);
#endif /* CONTROL_TOOLS_H_ */
