#ifndef INVERTER_MENU_H
#define INVERTER_MENU_H

#include <stdint.h>

void INV_ShowStandby(void);
void INV_Menu_Run(void);
/* 菜单参数下发到控制层：设置频率与目标电压，越界值由控制层钳位。 */
void INV_Menu_ApplyParams(int32_t frequency, float target_voltage);

#endif
