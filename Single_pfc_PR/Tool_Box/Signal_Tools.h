/*
 * Signal_Tools.h
 *
 *  Created on: Jul 9, 2025
 *      Author: HuaWenxi
 */
#include "main.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "arm_math.h"
#ifndef SIGNAL_TOOLS_H_
#define SIGNAL_TOOLS_H_
//----------------------FFT 华文熙编写-------------------
#define N 1024
/*复数结构体类型*/
typedef struct{
double realPart;
double imaginaryPart;
}complexNumber;
complexNumber x[N], W[N/2]; //序列数组以及旋转因子数组
int size_x = 256;           //输入序列的长度，只能为2的次幂
void fastFourierOperation();//快速傅里叶变换算法
void initRotationFactor();  //生成旋转因子数组
void changePosition();      //偶奇变换算法
void add(complexNumber ,complexNumber ,complexNumber*); //复数加法
void mul(complexNumber ,complexNumber ,complexNumber*); //复数乘法
void sub(complexNumber ,complexNumber ,complexNumber*); //复数减法



//----------------------FFT yuyang学长编写-------------------
typedef struct complex //��������
{
  float real;       //ʵ��
  float imag;       //�鲿
}complex;

complex c_plus(complex a,complex b);    // ������
complex c_mul(complex a,complex b);     // ������
complex c_sub(complex a,complex b);     // ��������
complex c_div(complex a,complex b);     // ��������
complex c_conjugate(complex a);         // ��������
float c_abs(complex a);                 // ������ģ
void cArray_abs(int size, complex *a, float *abs);   // ���������ģ
void fft(int size, complex *x, complex *X);     // ����Ҷ�任������size��x������X
void ifft(int size, complex *X, complex *x);    // ����Ҷ��任
#endif /* SIGNAL_TOOLS_H_ */
