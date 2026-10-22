#include "inverter_menu.h"
#include "inverter_control_tools.h"
#include "oled.h"
#include "Keyboard.h"
#include <stdio.h>

/* 128x64 屏，字模固定 8x16：每行 16 字符 / 128px，共 4 行。
 * 字符串超过 16 字符会被 OLED_ShowString 静默截断。
 *
 * 单页界面，7 个选项同屏：浏览态显示 FREQ/OUT、TGT/LOOP、AMP/KP、FB/RMS；
 * 编辑态第 4 行改为 KI/FB，避免 KI 调完看不见。2/8 在选项间循环，4/6 调整；
 * selected 为 -1 表示浏览态。
 */
enum
{
    MENU_FREQ, MENU_TGT, MENU_AMP, MENU_LOOP,
    MENU_KP, MENU_KI, MENU_FB, MENU_COUNT
};
#define MENU_NONE (-1)

static void DrawPage(int32_t selected)
{
    /* 显示按 16 字符截断，缓冲区留足 %ld/%lu 全量程，避免 snprintf 截断告警。 */
    char buffer[48];
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    float rms = INV_Vrms;
    int32_t enabled = INV_PwmFlag;
    int32_t frequency = INV_Freq, amplitude = INV_Amp;
    int32_t loop = INV_LoopEnable, feedback = INV_FeedbackChannel;
    float target = INV_TargetVoltage, kp = INV_VoltageKp, ki = INV_VoltageKi;
    __set_PRIMASK(primask);

    uint32_t target_tenths = (uint32_t)(target * 10.0f + 0.5f);
    uint32_t kp_hundredths = (uint32_t)(kp * 100.0f + 0.5f);
    uint32_t ki_tenths = (uint32_t)(ki * 10.0f + 0.5f);

    OLED_Clear();
    (void)snprintf(buffer, sizeof(buffer), "%cOUT:%s F:%ld",
                   selected == MENU_FREQ ? '>' : ' ',
                   enabled ? "ON " : "OFF", (long)frequency);
    OLED_ShowString(0, 0, buffer, 16);

    (void)snprintf(buffer, sizeof(buffer), "%cT:%lu.%luV %cL:%s",
                   selected == MENU_TGT ? '>' : ' ',
                   (unsigned long)(target_tenths / 10U),
                   (unsigned long)(target_tenths % 10U),
                   selected == MENU_LOOP ? '>' : ' ',
                   loop ? "CL" : "OP");
    OLED_ShowString(0, 16, buffer, 16);

    (void)snprintf(buffer, sizeof(buffer), "%cA:%ld%% %cKP:%lu.%02lu",
                   selected == MENU_AMP ? '>' : ' ', (long)amplitude,
                   selected == MENU_KP ? '>' : ' ',
                   (unsigned long)(kp_hundredths / 100U),
                   (unsigned long)(kp_hundredths % 100U));
    OLED_ShowString(0, 32, buffer, 16);

    if (selected == MENU_NONE)
    {
        /* 浏览态：实测输出有效值，窗口未满显示占位。 */
        if (rms < 0.0f)
            (void)snprintf(buffer, sizeof(buffer), "FB:V%ld RMS:--.-",
                           (long)feedback + 1L);
        else
        {
            uint32_t rms_tenths = (uint32_t)(rms * 10.0f + 0.5f);
            (void)snprintf(buffer, sizeof(buffer), "FB:V%ld RMS:%lu.%lu",
                           (long)feedback + 1L,
                           (unsigned long)(rms_tenths / 10U),
                           (unsigned long)(rms_tenths % 10U));
        }
    }
    else
    {
        /* 编辑态：第 4 行留给 KI 和反馈通道。 */
        (void)snprintf(buffer, sizeof(buffer), "%cKI:%lu.%lu %cFB:V%ld",
                       selected == MENU_KI ? '>' : ' ',
                       (unsigned long)(ki_tenths / 10U),
                       (unsigned long)(ki_tenths % 10U),
                       selected == MENU_FB ? '>' : ' ',
                       (long)feedback + 1L);
    }
    OLED_ShowString(0, 48, buffer, 16);
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
            selected = key == '8' ? selected + 1 : selected + MENU_COUNT - 1;
            selected %= MENU_COUNT;
        }
        else if (key == '4' || key == '6')
        {
            int32_t step = key == '6' ? 1 : -1;
            switch (selected)
            {
            case MENU_FREQ:
                INV_SetFrequency(INV_Freq + step);
                break;
            case MENU_TGT:
                INV_SetTargetVoltage(INV_TargetVoltage + 0.5f * step);
                break;
            case MENU_AMP:
                /* 闭环时幅值由 PI 控制，手动调整会被覆盖。 */
                if (!INV_LoopEnable) INV_SetAmplitude(INV_Amp + step);
                break;
            case MENU_LOOP:
                INV_SetLoopEnable(step > 0);
                break;
            case MENU_KP:
                INV_SetVoltageGains(
                    ((int32_t)(INV_VoltageKp * 100.0f + 0.5f) + step) / 100.0f,
                    INV_VoltageKi);
                break;
            case MENU_KI:
                INV_SetVoltageGains(INV_VoltageKp,
                    ((int32_t)(INV_VoltageKi * 10.0f + 0.5f) + step) / 10.0f);
                break;
            case MENU_FB:
                INV_SetFeedbackChannel(step > 0 ? 1 : 0);
                break;
            default:
                break;
            }
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
