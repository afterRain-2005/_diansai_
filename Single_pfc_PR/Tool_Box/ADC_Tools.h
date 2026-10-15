/*
 * ADC_Tools.h
 *
 *  Created on: Jul 23, 2025
 *      Author: HuaWenxi
 */

#ifndef ADC_TOOLS_H_
#define ADC_TOOLS_H_
#include "main.h"
#include "arm_math.h"
#include <stdio.h>
#include <stdlib.h>
#define maxsize 800
//----------------------------------锁相延时队列-------------------------------//
typedef struct SampQueue {
    float data[maxsize]; // 存储数据
    int front;           // 队头指针
    int rear;            // 队尾指针
} SampQueue;
// 采样队列
SampQueue initQueue(void);
int isEmpty(SampQueue qu);
int enQueue(SampQueue *qu, float x);//x入队
int deQueue(SampQueue *qu);//头元素出队
int printQueue(SampQueue qu);
int getFront(SampQueue *qu, float *val);//获取队头元素
int getRear(SampQueue *qu, float *val);//获取队尾元素
int queueLength(SampQueue *qu);
void clearQueue(SampQueue *qu);
#endif /* ADC_TOOLS_H_ */
