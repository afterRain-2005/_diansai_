/*
 * SPWM_Tools.c
 *
 *  Created on: Jul 24, 2025
 *      Author: HuaWenxi
 */
#include "Svpwm_Tools.h"

#define SQRT3       1.73205080757f
#define SQRT3_DIV2  0.86602540378f
#define TWO_DIV_SQRT3 1.15470053838f
// 七段式----------------SVPWM调制--------不太记得是中心模式还是up模式的代码了，好像两种计数方式都可以
void SVPWM_Calculate(SVPWM* svpwm, float v_alpha, float v_beta, float udc, uint32_t period)
{
    float v_ref;
    float angle;

    float t0, t1, t2;
    float ta, tb, tc;
    float m;

    v_ref = sqrtf(v_alpha * v_alpha + v_beta * v_beta);
    angle = atan2f(v_beta, v_alpha);

    m = (v_ref * TWO_DIV_SQRT3) / udc;
    if (m > 1.0f) {
        m = 1.0f;
    }

    if (angle >= 0 && angle < PI / 3.0f) svpwm->sector = 1;
    else if (angle >= PI / 3.0f && angle < 2.0f * PI / 3.0f) svpwm->sector = 2;
    else if (angle >= 2.0f * PI / 3.0f && angle < PI) svpwm->sector = 3;
    else if (angle >= -PI && angle < -2.0f * PI / 3.0f) svpwm->sector = 4;
    else if (angle >= -2.0f * PI / 3.0f && angle < -PI / 3.0f) svpwm->sector = 5;
    else svpwm->sector = 6;

    float angle_in_sector = angle - (svpwm->sector - 1) * (PI / 3.0f);
    t2 = m * arm_sin_f32(angle_in_sector);
    t1 = m * arm_sin_f32(PI / 3.0f - angle_in_sector);
    svpwm->t1 = t1;
    svpwm->t2 = t2;
    t0 = 1.0f - t1 - t2;

    switch (svpwm->sector)
    {
        case 1:
            ta = (t1 + t2 + t0 / 2.0f);
            tb = (t2 + t0 / 2.0f);
            tc = (t0 / 2.0f);
            break;
        case 2:
            ta = (t1 + t0 / 2.0f);
            tb = (t1 + t2 + t0 / 2.0f);
            tc = (t0 / 2.0f);
            break;
        case 3:
            ta = (t0 / 2.0f);
            tb = (t1 + t2 + t0 / 2.0f);
            tc = (t2 + t0 / 2.0f);
            break;
        case 4:
            ta = (t0 / 2.0f);
            tb = (t1 + t0 / 2.0f);
            tc = (t1 + t2 + t0 / 2.0f);
            break;
        case 5:
            ta = (t2 + t0 / 2.0f);
            tb = (t0 / 2.0f);
            tc = (t1 + t2 + t0 / 2.0f);
            break;
        case 6:
            ta = (t1 + t2 + t0 / 2.0f);
            tb = (t0 / 2.0f);
            tc = (t1 + t0 / 2.0f);
            break;
        default: // Should not happen
            ta = tb = tc = 0.5f;
            break;
    }
    // Ta_cmp1现在代表A相的比较值，不再需要Ta_cmp2
	svpwm->Ta_cmp1 = (uint32_t)(ta * period);
	svpwm->Tb_cmp1 = (uint32_t)(tb * period);
	svpwm->Tc_cmp1 = (uint32_t)(tc * period);

	// 确保比较值不超过周期值，限幅
	if (svpwm->Ta_cmp1 >= period) svpwm->Ta_cmp1 = period;
	if (svpwm->Tb_cmp1 >= period) svpwm->Tb_cmp1 = period;
	if (svpwm->Tc_cmp1 >= period) svpwm->Tc_cmp1 = period;

	// (可选) 为DAC输出计算相电压的部分可以保持不变
	svpwm->V_an = (ta - 0.5f) * udc;
	svpwm->V_bn = (tb - 0.5f) * udc;
	svpwm->V_cn = (tc - 0.5f) * udc;
}
/**
  * 五段式SVPWM调制（不连续PWM模式）一定要up计数模式
  */
void SVPWM_DPWM_Calculate(SVPWM* svpwm, float v_alpha, float v_beta, float udc, uint32_t period)
{
    float v_ref;
    float angle;
    float t1, t2;
    float ta, tb, tc;
    float m;

    // 计算参考电压幅值和角度
    v_ref = sqrtf(v_alpha * v_alpha + v_beta * v_beta);
    angle = atan2f(v_beta, v_alpha);

    // 计算调制比并限幅
    m = (v_ref * TWO_DIV_SQRT3) / udc;
    if (m > 1.0f) m = 1.0f;

    // 确定扇区 (1~6)
    if (angle >= 0 && angle < PI / 3.0f) svpwm->sector = 1;
    else if (angle >= PI / 3.0f && angle < 2.0f * PI / 3.0f) svpwm->sector = 2;
    else if (angle >= 2.0f * PI / 3.0f && angle < PI) svpwm->sector = 3;
    else if (angle >= -PI && angle < -2.0f * PI / 3.0f) svpwm->sector = 4;
    else if (angle >= -2.0f * PI / 3.0f && angle < -PI / 3.0f) svpwm->sector = 5;
    else svpwm->sector = 6;

    float angle_in_sector = angle - (svpwm->sector - 1) * (PI / 3.0f);
    t1 = m * arm_sin_f32(PI / 3.0f - angle_in_sector); // 注意与七段式t1/t2定义一致
    t2 = m * arm_sin_f32(angle_in_sector);
    svpwm->t1 = t1;
    svpwm->t2 = t2;

    // 根据扇区计算五段式占空比
    switch (svpwm->sector) {
        case 1: // A相固定高电平
            ta = 1.0f;
            tb = 1.0f - t1;
            tc = 1.0f - t1 - t2;
            break;
        case 2: // C相固定低电平
            ta = t1;
            tb = t1 + t2;
            tc = 0.0f;
            break;
        case 3: // B相固定高电平
            ta = 1.0f - t1 - t2;
            tb = 1.0f;
            tc = 1.0f - t1;
            break;
        case 4: // A相固定低电平
            ta = 0.0f;
            tb = t1;
            tc = t1 + t2;
            break;
        case 5: // C相固定高电平
            ta = 1.0f - t1;
            tb = 1.0f - t1 - t2;
            tc = 1.0f;
            break;
        case 6: // B相固定低电平
            ta = t1 + t2;
            tb = 0.0f;
            tc = t2;
            break;
        default: // 异常处理
            ta = tb = tc = 0.5f;
            break;
    }

    // 计算比较寄存器值（边沿对齐模式）
    svpwm->Ta_cmp1 = (uint32_t)(ta * period);
    svpwm->Tb_cmp1 = (uint32_t)(tb * period);
    svpwm->Tc_cmp1 = (uint32_t)(tc * period);

    // 比较值限幅 (0 ~ period)
    if (svpwm->Ta_cmp1 > period) svpwm->Ta_cmp1 = period;
    if (svpwm->Tb_cmp1 > period) svpwm->Tb_cmp1 = period;
    if (svpwm->Tc_cmp1 > period) svpwm->Tc_cmp1 = period;

    // 计算相电压（可选，用于监控）
    svpwm->V_an = (ta - 0.5f) * udc;
    svpwm->V_bn = (tb - 0.5f) * udc;
    svpwm->V_cn = (tc - 0.5f) * udc;
}
