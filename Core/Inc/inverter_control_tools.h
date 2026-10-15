#ifndef INVERTER_CONTROL_TOOLS_H
#define INVERTER_CONTROL_TOOLS_H

#include <stdint.h>

typedef struct
{
    float kp;
    float ki;
    float integral;
    float output;
    float minimum;
    float maximum;
} INV_PI_Controller;

void INV_PI_Init(INV_PI_Controller *controller, float kp, float ki,
                 float minimum, float maximum);
float INV_PI_Update(INV_PI_Controller *controller, float error);
void INV_PI_Reset(INV_PI_Controller *controller);

void INV_Clarke(float phase_a, float phase_b, float phase_c,
                float *alpha, float *beta);
void INV_Park(float alpha, float beta, float sine, float cosine,
              float *direct, float *quadrature);
void INV_InvPark(float direct, float quadrature, float sine, float cosine,
                 float *alpha, float *beta);
void INV_InvClarke(float alpha, float beta,
                   float *phase_a, float *phase_b, float *phase_c);

#endif
