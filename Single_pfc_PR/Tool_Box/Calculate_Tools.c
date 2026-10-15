/*
 * Calculate_Tools.c
 *
 *  Created on: Jun 25, 2025
 *      Author: HuaWenxi
 */

#include "Calculate_tools.h"
/* 将输入值限制在 [min, max] 区间内。
 * 当 input < min 时返回 min，若 input > max 则返回 max，否则返回 input*/
float fix(float input, float min, float max)
{
    return input < min ? min : (input > max ? max : input);
}
/* 基于Quake III的快速平方根算法
 * x是要开方的浮点数
 * 返回x的平方根近似值*/
float qsqrt(float x)
{
    union {
        float f;
        uint32_t i;
    } conv;
    conv.f = x;
    conv.i = 0x5f3759df - (conv.i >> 1);
    float y = conv.f;
    float xhalf = 0.5f * x;
    y = y * (1.5f - xhalf * y * y);
    return 1.0f / y;
}
/* 计算base^n的递归函数，n为非负整数。
 * base为底数
 * n为指数
 * 返回base^n*/
float qpow(float base, int n)
{
    // 若 n=0，则任何数的 0 次方为 1
    if (n == 0)
        return 1;
    // 若 n 为奇数，则先递归计算 base^(n-1)，再乘以 base
    else if (n % 2 == 1)
        return qpow(base, n - 1) * base;
    // 若 n 为偶数，则递归计算 base^(n/2) 再将结果平方
    else
    {
        float temp = qpow(base, n / 2);
        return temp * temp;
    }
}
float Cal_dc(float* U_ac,int n)
{
	float max,min;
	max = 0;
	min = 3.3;
	for(int i = 1;i<n;i++)
	{
		if(U_ac[i]>=max)
		{
			max=U_ac[i];
		}
		if(U_ac[i]<=min)
		{
			min=U_ac[i];
		}
	}
	return (max+min)/2.0f;
}
