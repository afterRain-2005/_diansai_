#ifndef __INVERTER_MENU_H
#define __INVERTER_MENU_H

#include "main.h"
#include <stdint.h>

#define INV_FLASH_SLOT_SIZE  0x1000UL
#define INV_FLASH_ADDR       0x0807E000UL
#define INV_FLASH_MAGIC      0xAA55C0DFUL
#define INV_FLASH_VERSION    1UL

void INV_Menu_Init(void);
HAL_StatusTypeDef INV_Menu_Save(void);
void INV_ShowStandby(void);
void INV_Menu_Run(void);

#endif
