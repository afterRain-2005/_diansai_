/*
 * ADC_Tools.c
 *
 *  Created on: Jul 23, 2025
 *      Author: HuaWenxi
 */
#include "ADC_Tools.h"
//----------------------------------------------------------队列存储------------------------------------------------------//
/**
 * 队列函数
 */
SampQueue initQueue() {
    SampQueue *sq = (SampQueue *)malloc(sizeof(SampQueue));
    sq->rear = sq->front = 0;
    return *sq;
}
// 判断队列是否为空
int isEmpty(SampQueue qu) {
    return (qu.front == qu.rear ? 1 : 0);
}
// 入队操作，队满时自动删除队头
int enQueue(SampQueue *qu, float x) {
    if ((qu->rear + 1) % maxsize == qu->front) { // 队满，删除队头
        deQueue(qu);
    }
    qu->data[qu->rear] = x;
    qu->rear = (qu->rear + 1) % maxsize;
    return 1;
}
// 出队操作，删除队头元素
int deQueue(SampQueue *qu) {
    if (qu->rear == qu->front) { // 队空
        return 0;
    }
    qu->front = (qu->front + 1) % maxsize;
    return 1;
}
// 打印队列中的所有元素
int printQueue(SampQueue qu) {
    if (qu.rear == qu.front) {
        return 0;
    }
    int i = qu.front;
    while (i != qu.rear) {
        printf("%f\r\n", qu.data[i]);
        i = (i + 1) % maxsize;
    }
    return 1;
}
// 获取队头元素，无删除（成功返回1，*val保存值，失败返回0）
int getFront(SampQueue *qu, float *val) {
    if (qu->rear == qu->front) {
        return 0;
    }
    *val = qu->data[qu->front];
    return 1;
}
// 获取队尾元素，无删除（成功返回1，*val保存值，失败返回0）
int getRear(SampQueue *qu, float *val) {
    if (qu->rear == qu->front) {
        return 0;
    }
    int idx = (qu->rear - 1 + maxsize) % maxsize;
    *val = qu->data[idx];
    return 1;
}
// 返回队列长度
int queueLength(SampQueue *qu) {
    return (qu->rear - qu->front + maxsize) % maxsize;
}
// 清空队列
void clearQueue(SampQueue *qu) {
    qu->front = qu->rear = 0;
}
