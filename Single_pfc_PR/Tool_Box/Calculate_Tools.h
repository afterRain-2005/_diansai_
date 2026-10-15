/*
 * Calculate_Tools.h
 *
 *  Created on: Jun 25, 2025
 *      Author: HuaWenxi
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#ifndef CALCULATE_TOOLS_H_
#define CALCULATE_TOOLS_H_


float fix(float input,float min,float max);
float qsqrt(float x);
float qpow(float base,int n);
float Cal_dc(float* U_ac,int n);

#endif /* CALCULATE_TOOLS_H_ */
