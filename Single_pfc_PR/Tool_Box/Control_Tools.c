/*
 * PLL_Control.c
 *
 *  Created on: Jun 23, 2025
 *      Author: HuaWenxi
 */
#include "Control_Tools.h"
#include "main.h"
#include "arm_math.h"
//----------------------------------------------------------Delta_PI------------------------------------------------------//
/**
 * 增量式PI初始化
 */
void PI_Delta_Init(PI_Controller *Pi, float kp, float ki, float min, float max)
{
    Pi->Kp = kp;
    Pi->Ki = ki;
    Pi->Delta_Min = min;
    Pi->Delta_Max = max;
    Pi->Error = 0.0f;
    Pi->Error_last = 0.0f;
}
/**
 * 增量式PI计算
 */
void PI_Delta_Cal(PI_Controller *Pi, float Pi_in)
{
    float pError;

    pError = Pi_in - Pi->Error_last;

    // PID计算
    Pi->Pi_Delta = (Pi->Kp * pError
                    + Pi->Ki * Pi_in);

    // 限幅
    if (Pi->Pi_Delta > Pi->Delta_Max|| Pi->Pi_Delta < Pi->Delta_Min){
        Pi->Error_last = Pi_in;
        Pi->Pi_Delta = (Pi->Pi_Delta > Pi->Delta_Max)? Pi->Delta_Max : Pi->Delta_Min;
    }
    else{
    	Pi->Error_last = Pi_in;
    }
}
//----------------------------------------------------------Pos_PI------------------------------------------------------//
/**
 * 位置式PI初始化
 */
void PI_Pos_Init(PI_Controller *Pi, float kp, float ki, float min, float max)
{
    Pi->Kp = kp;
    Pi->Ki = ki;
    Pi->integral = 0.0f;
    Pi->integral_min = min;
    Pi->integral_max = max;
    Pi->Error = 0.0f;
    Pi->Pos = 0.0f;
}
/**
 * 位置式PI计算
 */
void PI_Pos_Cal(PI_Controller *Pi, float error)
{
	Pi->Error = Pi->Kp * error;
	float integral_temp = Pi->integral + Pi->Ki * error;
	float output_temp = Pi->Error + integral_temp;
	if (output_temp > Pi->integral_max) {
		Pi->Pos = Pi->integral_max;
	} else if (output_temp < Pi->integral_min) {
		Pi->Pos = Pi->integral_min;
	} else {
		Pi->integral = integral_temp;
		Pi->Pos = output_temp;
	}
	if (Pi->integral > Pi->integral_max) {
		Pi->integral = Pi->integral_max;
	} else if (Pi->integral < Pi->integral_min) {
		Pi->integral = Pi->integral_min;
	}
}
//----------------------------------------------------------PR------------------------------------------------------//
/**
 * PR谐振滤波初始化（T为周期，wc是中心频率，w0是带宽，中心频率是对应频率乘2pi，w0是带宽，一般比较小）
 */
void PR_Init(PR_Controller* ctrl, float T, float Kp, float Kr,float Wc, float W0, float out_limit) {
    float a0 = (4.0f / (T * T)) + (4.0f * Wc / T) + W0 * W0;
    float a1 = (-8.0f / (T * T)) + 2.0f * W0 * W0;
    float a2 = (4.0f / (T * T)) - (4.0f * Wc / T) + W0 * W0;
    float b0 = (4.0f * Kp) / (T * T) + (4.0f * Wc * (Kp + Kr) / T) + Kp * W0 * W0;
    float b1 = (-8.0f * Kp) / (T * T) + 2.0f * Kp * W0 * W0;
    float b2 = (4.0f * Kp) / (T * T) - (4.0f * Wc * (Kp + Kr) / T) + Kp * W0 * W0;
    ctrl->T=T;
    ctrl->kp=Kp;
    ctrl->kr=Kr;
    ctrl->W0=W0;
    ctrl->Wc=Wc;
    ctrl->b0 = b0 / a0;
    ctrl->b1 = b1 / a0;
    ctrl->b2 = b2 / a0;
    ctrl->a1 = a1 / a0;
    ctrl->a2 = a2 / a0;
    ctrl->u_prev1 = 0.0f;
    ctrl->u_prev2 = 0.0f;
    ctrl->y_prev1 = 0.0f;
    ctrl->y_prev2 = 0.0f;
    ctrl->output=0;
    ctrl->out_limit=out_limit;
}
/**
 * PR谐振滤波计算
 */
void PR_Cal(PR_Controller* ctrl, float input, float gain) {
	ctrl->output =
        ctrl->b0 * input +
        ctrl->b1 * ctrl->u_prev1 +
        ctrl->b2 * ctrl->u_prev2 -
        ctrl->a1 * ctrl->y_prev1 -
        ctrl->a2 * ctrl->y_prev2;
    ctrl->u_prev2 = ctrl->u_prev1;
    ctrl->u_prev1 = input;
    ctrl->y_prev2 = ctrl->y_prev1;
    ctrl->y_prev1 = ctrl->output;

	ctrl->output*=gain;
	ctrl->output = ctrl->output > ctrl->out_limit ? ctrl->out_limit :
			(ctrl->output < -ctrl->out_limit ? -ctrl->out_limit : ctrl->output);
}
//----------------------------------------------------------Clark & inv_Clark------------------------------------------------------//
/**
 * Clark变换计算
 */
void Clarke_F(float Ua, float Ub, float Uc, float *Ualpha, float *Ubeta)
{
    // α分量 = (2/3) * [Ua - 0.5*(Ub+Uc)]
    *Ualpha = (0.6666666667f) * (Ua-(Ub+Uc)/2.0f);
    // β分量 = (1/√3) * (Ub-Uc)
    *Ubeta = (0.5773502692f) * (Ub-Uc);
}
/**
 * 反Clark变换计算
 */
void inv_Clarke_F(float Udc, float Ualpha, float Ubeta, float *w1, float *w2, float *w3)
{
	//用ARR计数器的一半来做归一化，记得要改成输出PWM的定时器！！！！！！！！！！！！！
    float k = (float)(TIM1->ARR + 1)/ Udc/ 2;
    *w1 = k * (Udc + 2.0f * Ualpha);
    *w2 = k * (Udc - Ualpha + sqrtf(3.0f) * Ubeta);
    *w3 = k * (Udc - Ualpha - sqrtf(3.0f) * Ubeta);
}
//----------------------------------------------------------AC状态检测------------------------------------------------------//
/**
 * 交流状态初始化
 */
void AC_State_init(AC_State *AC_Discern_Pamer)
{
	AC_Discern_Pamer->AC_State = 0;      //0相位起始
	AC_Discern_Pamer->derta_VAC = 0.01f; //比较电平
}
/**
 * 交流状态检测
 */
void AC_State_Detect(AC_State *AC_Discern_Pamer, float Vac_PLL)
{
	float derta_V = AC_Discern_Pamer->derta_VAC;
	if(AC_Discern_Pamer->AC_State == 0 && Vac_PLL >= derta_V)
	{// （0，180）
		AC_Discern_Pamer->AC_State = 1;
	}
	else if(AC_Discern_Pamer->AC_State == 1 && (Vac_PLL < derta_V && Vac_PLL > -derta_V))
	{//    180
		AC_Discern_Pamer->AC_State = 2;
	}
	else if(AC_Discern_Pamer->AC_State == 2 && Vac_PLL <= -derta_V*2)
	{// （180，360）
		AC_Discern_Pamer->AC_State = 3;
	}
	else if(AC_Discern_Pamer->AC_State == 3 && (Vac_PLL > -derta_V && Vac_PLL < derta_V))
	{//   360
		AC_Discern_Pamer->AC_State = 4;
	}
	else if(AC_Discern_Pamer->AC_State == 4 && Vac_PLL >= derta_V*2)
	{// （0，180）
		AC_Discern_Pamer->AC_State = 1;
	}
}
//----------------------------------------------------------锁相环------------------------------------------------------//
/**
 * 锁相环初始化
 *///sample频率为20KHz，nominal频率为50Hz
void PLL_Init(PLL_Controller* pll, float kp, float ki, float sample_freq, float nominal_freq)
{
    pll->ts = 1.0f / sample_freq;
    pll->ws_cen = 2.0f * PI * nominal_freq;
    pll->ws = 0.0f;
    pll->ud = 0.0f;
    pll->uq = 0.0f;
    pll->sin_theta = 0.0f;
    pll->cos_theta = 1.0f;
    PI_Pos_Init(&pll->pi, kp, ki, -50.0f, 330.0f); // 限幅值需要根据实际情况调整
}
/**
 * 锁相环计算
 *///Alpha和Beta为传进来的数据，可由T/4计算或者Sogi计算
void PLL_Update(PLL_Controller* pll, float alpha, float beta)
{
    arm_park_f32(alpha, beta, &pll->ud, &pll->uq, pll->sin_theta, pll->cos_theta);
    // 锁相控制Uq为0
    PI_Pos_Cal(&pll->pi, pll->uq);
    pll->ws_cen = pll->pi.Pos;
    // 限幅
    if (pll->ws_cen > 2.0f * PI * 55.0f) pll->ws_cen = 2.0f * PI * 55.0f;
    if (pll->ws_cen < 2.0f * PI * 45.0f) pll->ws_cen = 2.0f * PI * 45.0f;
    pll->ws += pll->ws_cen * pll->ts;
    // 归一化
    if (pll->ws > 2.0f * PI) {
        pll->ws -= 2.0f * PI;
    } else if (pll->ws < 0.0f) {
        pll->ws += 2.0f * PI;
    }
    // 锁相输出
    pll->sin_theta = arm_sin_f32(pll->ws);
    pll->cos_theta = arm_cos_f32(pll->ws);
}
//----------------------------------------------------------Sogi------------------------------------------------------//
/**
 * Sogi初始化
 */
void sogi_pll_init(SOGI_PLL_DATA_DEF *spll_obj, float grid_freq)
{
    spll_obj->freq = grid_freq;

    spll_obj->sogi_u_coeff.gain = SIGO_U_GAIN;
    spll_obj->sogi_u_coeff.B0 = SIGO_U_B0;
    spll_obj->sogi_u_coeff.B1 = SIGO_U_B1;
    spll_obj->sogi_u_coeff.B2 = SIGO_U_B2;

    spll_obj->sogi_u_coeff.A1 = SIGO_U_A1;
    spll_obj->sogi_u_coeff.A2 = SIGO_U_A2;

    spll_obj->sogi_qu_coeff.gain = SIGO_QU_GAIN;
    spll_obj->sogi_qu_coeff.B0 = SIGO_QU_B0;
    spll_obj->sogi_qu_coeff.B1 = SIGO_QU_B1;
    spll_obj->sogi_qu_coeff.B2 = SIGO_QU_B2;

    spll_obj->sogi_qu_coeff.A1 = SIGO_QU_A1;
    spll_obj->sogi_qu_coeff.A2 = SIGO_QU_A2;
}
/**
 * Sogi双线性变换计算
 */
float discrete_2order_tf(const float input, DIS_2ORDER_TF_COEF_DEF *coeff, DIS_2ORDER_TF_DATA_DEF *data)
{
    // w0 = x(0) - A1 * W1 - A2 * W2
    data->w0 = input - coeff->A1 * data->w1 - coeff->A2 * data->w2;

    // Y(0) = Gain * (B0 * W0 + B1 * W1 + B2 * W2)
    data->output = coeff->gain * (coeff->B0 * data->w0 + coeff->B1 * data->w1 + coeff->B2 * data->w2);

    data->w2 = data->w1;
    data->w1 = data->w0;

    return (data->output);
}
/**
 * Sogi计算，得到的Ualpha和Ubeta放到上面的PLL函数就可以实现锁相环了
 * Sogi自带滤波效果，主要用于那些电网质量较差的时候，锁相效果较好
 * 如果电网质量正常，建议T/4，简单实用
 */
void spll_sogi_func(SOGI_PLL_DATA_DEF *spll_obj, float32_t grid_volt_sen)
{
    // signal genertor
    spll_obj->Ualpha = discrete_2order_tf(grid_volt_sen, &(spll_obj->sogi_u_coeff), &(spll_obj->sogi_u_data));
    spll_obj->Ubeta = discrete_2order_tf(grid_volt_sen, &(spll_obj->sogi_qu_coeff), &(spll_obj->sogi_qu_data));
}
//----------------------------------------------------------MPPT最大功率计算------------------------------------------------------//
/**
 * 增量式MPPT初始化
 */
void MPPTDeltaInit(MPPT_PNO_F *v, float Ipv, float Vpv, float DeltaPmin, float MaxVolt, float MinVolt,float Stepsize, float PanelPower_Prev)
{
   v->Ipv = Ipv;
   v->Vpv = Vpv;
   v->DeltaPmin = DeltaPmin;
   v->MaxVolt = MaxVolt;
   v->MinVolt = MinVolt;
   v->Stepsize = Stepsize;
   v->VmppOut = 0;
   v->DeltaP = 0;
   v->LastDeltaP = 0;
   v->PanelPower = PanelPower_Prev;
   v->PanelPower_Prev = 0;
   v->mppt_enable = 1;
   v->mppt_first = 1;
}
/**
 * 增量式MPPT计算
 */
void MPPT_PNO_F_FUNC(MPPT_PNO_F *v)
{
//    float iError;
    if (v->mppt_enable==1)
    {
        if (v->mppt_first == 1)
        {
            v->VmppOut = v->Vpv - 1;
            v->mppt_first = 0;
            v->PanelPower_Prev = v->PanelPower;
        }
        else
        {
            v->PanelPower = (v->Vpv*v->Ipv);;
            v->DeltaP = v->PanelPower - v->PanelPower_Prev;
//            iError = v->DeltaP - v->LastDeltaP;
//            v->Stepsize = (v->DeltaP + 0.1*iError);
            if (v->DeltaP > v->DeltaPmin)
            {
                v->VmppOut = v->Vpv + v->Stepsize;
            }
            else
            {
                if (v->DeltaP < -v->DeltaPmin)
                {
                    v->Stepsize = -v->Stepsize;
                    v->VmppOut = v->Vpv + v->Stepsize;
                }
            }
//            v->LastDeltaP = v->DeltaP;
            v->PanelPower_Prev = v->PanelPower;
        }
        if(v->VmppOut < v->MinVolt) v->VmppOut = v->MinVolt;
        if(v->VmppOut > v->MaxVolt) v->VmppOut = v->MaxVolt;
    }
}
/**
 * incc式MPPT初始化
 */
void MPPT_INCC_F_init(MPPT_INCC_F *v,float Ipv,float Vpv,float MaxVolt,float MinVolt,float Stepsize)
{
    v->Ipv=Ipv;
    v->Vpv=Vpv;
    v->IpvH=(1.65);
    v->IpvL=0;
    v->VpvH=0;
    v->VpvL=0;
    v->MaxVolt=MaxVolt;
    v->MinVolt=MinVolt;
    v->Stepsize=Stepsize;
    v->VmppOut=0;
    v->Cond=0;
    v->IncCond=0;
    v->IncCond2 = 0;
    v->DeltaV=0;
    v->DeltaI=0;
    v->VpvOld=0;
    v->IpvOld=0;
    v->StepFirst=(0.02);
    v->mppt_enable=1;
    v->mppt_first=1;
}

/**
 * incc式MPPT计算
 */
void MPPT_INCC_F_FUNC(MPPT_INCC_F *v)
{
    if (v->mppt_enable==1)
    {   /*MPPT mode enable*/
        if (v->mppt_first == 1)
        {
            v->VmppOut= v->Vpv - v->StepFirst;
            v->VpvOld=v->Vpv;
            v->IpvOld=v->Ipv;
            v->mppt_first=0;
        }
        else
        {
            v->DeltaV = v->Vpv-v->VpvOld ;              /* PV voltage change */
            v->DeltaI = v->Ipv-v->IpvOld ;              /* PV current change */
//          if(v->DeltaV <v->VpvH && v->DeltaV >-v->VpvL )  /* Tolerence limits PV Voaltge Variation */
//          v->DeltaV= 0;
//          if(v->DeltaI <v->IpvH && v->DeltaI >-v->IpvL )  /* Tolerence limits PV current Variation */
//          v->DeltaI = 0;
            if(v->DeltaV == 0)                          /* DeltaV=0 */
            {
                if(v->DeltaI !=0 )                      /* dI=0 operating at MPP */
                {
                    if ( v->DeltaI > 0 )                /* climbing towards MPP */
                    {v->VmppOut=v->Vpv+v->Stepsize;}        /* Increment Vref */
                    else
                    {v->VmppOut=v->Vpv-v->Stepsize;}        /* Decrement Vref */
                }
            }
            else
            {
                v->Cond = v->Ipv/v->Vpv;                 /*Cond =-(v->Ipv<<10)/v->Vpv;*/
//                v->IncCond = v->DeltaI/v->DeltaV;            /*IncCond =(DeltaI<<10)/DeltaV;*/
                if (v->IncCond != v->Cond)              /* Operating at MPP */
                {
                    if (v->IncCond > (-v->Cond))            /* left of MPP */
                    {
                    v->VmppOut=v->Vpv+v->Stepsize;          /* change Vref */
                    }
                    else                                /* Right of MPP */
                    {
                    v->VmppOut=v->Vpv-v->Stepsize;          /* change Vref */
                    }
                }
            }
            v->VpvOld = v->Vpv;
            v->IpvOld = v->Ipv;
        }
        if(v->VmppOut < v->MinVolt) v->VmppOut = v->MinVolt;
        if(v->VmppOut > v->MaxVolt) v->VmppOut = v->MaxVolt;
    }
}




