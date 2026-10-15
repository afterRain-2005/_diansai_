#include "inverter_menu.h"
#include "inverter_control_tools.h"
#include "oled.h"
#include "Keyboard.h"
#include <stdio.h>

void INV_ShowStandby(void)
{
    char buffer[24];

    OLED_Clear();
    (void)snprintf(buffer, sizeof(buffer), "INV CMD:%s", INV_PwmFlag ? "ON" : "OFF");
    OLED_ShowString(0, 0, buffer, 16);
    (void)snprintf(buffer, sizeof(buffer), "F:%ldHZ A:%ld%%", (long)INV_Freq, (long)INV_Amp);
    OLED_ShowString(0, 16, buffer, 16);
    OLED_ShowString(0, 32, "OPEN LOOP SPWM", 16);
    OLED_ShowString(0, 48, "A:ON C:OFF D:SET", 16);
    OLED_Refresh();
}

static void DrawMenu(uint8_t selected)
{
    char buffer[24];

    OLED_Clear();
    OLED_ShowString(0, 0, "SET 2/8:SELECT", 16);
    (void)snprintf(buffer, sizeof(buffer), "%cFREQ:%ld HZ", selected == 0U ? '>' : ' ', (long)INV_Freq);
    OLED_ShowString(0, 16, buffer, 16);
    (void)snprintf(buffer, sizeof(buffer), "%cAMP:%ld %%", selected == 1U ? '>' : ' ', (long)INV_Amp);
    OLED_ShowString(0, 32, buffer, 16);
    OLED_ShowString(0, 48, "4:- 6:+ D:BACK", 16);
    OLED_Refresh();
}

void INV_Menu_Run(void)
{
    uint8_t selected = 0U;

    INV_OutputDisable();
    DrawMenu(selected);
    while (1)
    {
        char key = Keyboard_GetKey();
        if (key == 'C' || key == 'D') return;
        if (key == '2' || key == '8') selected ^= 1U;
        else if (key == '4' || key == '6')
        {
            int32_t step = key == '6' ? 1 : -1;
            if (selected == 0U) INV_SetFrequency(INV_Freq + step);
            else INV_SetAmplitude(INV_Amp + step);
        }
        if (key != KEYBOARD_NO_KEY) DrawMenu(selected);
        HAL_Delay(1);
    }
}
