#ifndef INVERTER_MENU_H
#define INVERTER_MENU_H

#include <stdint.h>

/* 1 ms 中断内调用：读取按键、更新菜单状态；不操作 SPI/OLED。 */
void INV_Menu_TickISR(void);
/* 主循环开始前调用：复位菜单状态并允许按键处理。 */
void INV_Menu_Init(void);
/* 主循环调用：有刷新请求时返回 1，并清除请求。 */
uint8_t INV_Menu_TakeUpdate(void);
/* 主循环调用：按当前状态刷新 OLED。 */
void INV_Menu_Draw(void);

#endif
