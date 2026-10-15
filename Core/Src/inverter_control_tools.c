#include "inverter_control_tools.h"
#include <math.h>

void INV_PI_Init(INV_PI_Controller *controller, float kp, float ki,
                 float minimum, float maximum)
{
    if (controller == 0) return;
    controller->kp = isfinite(kp) && kp >= 0.0f ? kp : 0.0f;
    controller->ki = isfinite(ki) && ki >= 0.0f ? ki : 0.0f;
    controller->minimum = minimum < maximum ? minimum : 0.0f;
    controller->maximum = maximum > minimum ? maximum : 0.0f;
    INV_PI_Reset(controller);
}

void INV_PI_Reset(INV_PI_Controller *controller)
{
    if (controller == 0) return;
    controller->integral = 0.0f;
    controller->output = 0.0f;
}

float INV_PI_Update(INV_PI_Controller *controller, float error)
{
    float integral;
    float output;

    if (controller == 0 || !isfinite(error)) return 0.0f;
    integral = controller->integral + controller->ki * error;
    if (integral > controller->maximum) integral = controller->maximum;
    if (integral < controller->minimum) integral = controller->minimum;
    output = controller->kp * error + integral;
    if (output > controller->maximum)
    {
        output = controller->maximum;
    }
    else if (output < controller->minimum)
    {
        output = controller->minimum;
    }
    else
    {
        controller->integral = integral;
    }
    controller->output = output;
    return output;
}

void INV_Clarke(float phase_a, float phase_b, float phase_c,
                float *alpha, float *beta)
{
    if (alpha == 0 || beta == 0) return;
    *alpha = (2.0f / 3.0f) * (phase_a - 0.5f * (phase_b + phase_c));
    *beta = (phase_b - phase_c) * 0.57735026919f;
}

void INV_Park(float alpha, float beta, float sine, float cosine,
              float *direct, float *quadrature)
{
    if (direct == 0 || quadrature == 0) return;
    *direct = alpha * cosine + beta * sine;
    *quadrature = beta * cosine - alpha * sine;
}

void INV_InvPark(float direct, float quadrature, float sine, float cosine,
                 float *alpha, float *beta)
{
    if (alpha == 0 || beta == 0) return;
    *alpha = direct * cosine - quadrature * sine;
    *beta = direct * sine + quadrature * cosine;
}

void INV_InvClarke(float alpha, float beta,
                   float *phase_a, float *phase_b, float *phase_c)
{
    if (phase_a == 0 || phase_b == 0 || phase_c == 0) return;
    *phase_a = alpha;
    *phase_b = -0.5f * alpha + 0.86602540378f * beta;
    *phase_c = -0.5f * alpha - 0.86602540378f * beta;
}
