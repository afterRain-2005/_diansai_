#include "inverter_menu.h"
#include "inverter_control_tools.h"
#include "oled.h"
#include "Keyboard.h"
#include <stdio.h>

/* 128x64 屏，字模固定 8x16：每行 16 字符 / 128px，共 4 行。
 * 字符串超过 16 字符会被 OLED_ShowString 静默截断。
 *
 * 单页界面：状态、频率、幅度全部同屏显示，不分菜单页。
 * selected 为 -1 表示浏览态（无选中标记），0/1 表示正在编辑 FREQ/AMP。
 */
#define MENU_STR_MAX   16
#define MENU_NONE      (-1)

static void DrawPage(int32_t selected)
{
    char buffer[MENU_STR_MAX + 1];

    OLED_Clear();
    (void)snprintf(buffer, sizeof(buffer), "OUT:%s %cF:%ldHZ",
                   INV_PwmFlag ? "ON " : "OFF",
                   selected == 0 ? '>' : ' ', (long)INV_Freq);
    OLED_ShowString(0, 0, buffer, 16);

    (void)snprintf(buffer, sizeof(buffer), "%cT:%ld.%ldV A:%ld%%",
                   selected == 1 ? '>' : ' ',
                   (long)(int32_t)INV_TargetVoltage,
                   (long)(int32_t)(INV_TargetVoltage * 10.0f) % 10,
                   (long)INV_Amp);
    OLED_ShowString(0, 16, buffer, 16);

    float rms = INV_GetVoltageRms();
    if (rms >= 0.0f)
        (void)snprintf(buffer, sizeof(buffer), "RMS:%ld.%02ldV",
                       (long)(int32_t)rms,
                       (long)(int32_t)(rms * 100.0f) % 100);
    else
        (void)snprintf(buffer, sizeof(buffer), "RMS:--.--V");
    OLED_ShowString(0, 32, buffer, 16);
    OLED_ShowString(0, 48, "A:ON C:OFF D:EXIT", 16);
    OLED_Refresh();
}

void INV_ShowStandby(void)
{
    DrawPage(MENU_NONE);
}

void INV_Menu_Run(void)
{
    int32_t selected = 0;

    INV_OutputDisable();
    DrawPage(selected);

    while (1)
    {
        char key = Keyboard_GetKey();

        if (key == 'D') return;                 /* 退出编辑，回浏览态 */
        if (key == 'C') { INV_OutputDisable(); return; }

        if (key == '2' || key == '8')
        {
            selected ^= 1;                      /* 在 FREQ / AMP 间切换 */
        }
        else if (key == '4' || key == '6')
        {
            int32_t step = key == '6' ? 1 : -1;
            if (selected == 0) INV_SetFrequency(INV_Freq + step);
            else               INV_SetTargetVoltage(INV_TargetVoltage + 0.5f * step);
        }
        else if (key == 'A')
        {
            if (INV_PwmFlag) INV_OutputDisable();
            else             INV_OutputEnable();
        }

        if (key != KEYBOARD_NO_KEY) DrawPage(selected);
        HAL_Delay(1);
    }
}