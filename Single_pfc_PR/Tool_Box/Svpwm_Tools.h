/*
 * SPWM_Tools.h
 *
 *  Created on: Jul 24, 2025
 *      Author: HuaWenxi
 */

#ifndef SVPWM_TOOLS_H_
#define SVPWM_TOOLS_H_
#include "main.h"
#include "arm_math.h"

// SVPWM模块结构体
typedef struct {

    uint32_t Ta_cmp1, Ta_cmp2;
    uint32_t Tb_cmp1, Tb_cmp2;
    uint32_t Tc_cmp1, Tc_cmp2;

    float    V_an;
    float    V_bn;
    float    V_cn;

    uint8_t  sector;
    float    t1, t2;
} SVPWM;
// SVPWM调制----------------七段式
void SVPWM_Calculate(SVPWM* svpwm, float v_alpha, float v_beta, float udc, uint32_t period);
// SVPWM调制----------------五段式
void SVPWM_DPWM_Calculate(SVPWM* svpwm, float v_alpha, float v_beta, float udc, uint32_t period);
#endif /* SVPWM_TOOLS_H_ */
