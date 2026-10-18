#include "inverter_menu.h"
#include "inverter_control_tools.h"
#include "oled.h"
#include "Keyboard.h"
#include <stdio.h>

/* 128x64 屏，字模固定 8x16：每行 16 字符 / 128px，共 4 行。
 * 字符串超过 16 字符会被 OLED_ShowString 静默截断。
 *
 * 单页界面，5 个选项同屏：FREQ/OUT、TGT/LOOP、AMP、FB/RMS。
 * 2/8 在选项间循环，4/6 调整；menu_selected 为 -1 表示浏览态。
 * 按键和菜单状态在 1 ms 中断里处理，OLED 刷新留在主循环，
 * 避免 SPI 阻塞打断 TIM5/ADC 中断。
 * Kp/Ki 固定为 inverter_control_tools.h 的宏，不在此调整。
 */
enum
{
    MENU_FREQ, MENU_TGT, MENU_AMP, MENU_LOOP,
    MENU_FB, MENU_COUNT
};
#define MENU_NONE (-1)

/* 中断写、主循环读；读数时屏蔽中断。 */
static volatile int32_t menu_selected = MENU_NONE;
static volatile uint8_t menu_dirty = 1U;
static volatile uint8_t menu_running;
static uint32_t menu_refresh_ms;

static void DrawPage(void)
{
    char buffer[48];
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    int32_t selected = menu_selected;
    float rms = INV_Vrms;
    int32_t enabled = INV_PwmFlag;
    int32_t frequency = INV_Freq, amplitude = INV_Amp;
    int32_t loop = INV_LoopEnable, feedback = INV_FeedbackChannel;
    float target = INV_TargetVoltage;
    __set_PRIMASK(primask);

    uint32_t target_tenths = (uint32_t)(target * 10.0f + 0.5f);

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

    (void)snprintf(buffer, sizeof(buffer), "%cA:%ld%%",
                   selected == MENU_AMP ? '>' : ' ', (long)amplitude);
    OLED_ShowString(0, 32, buffer, 16);

    /* 第 4 行：反馈通道和实测 RMS，窗口未满显示占位。 */
    if (rms < 0.0f)
        (void)snprintf(buffer, sizeof(buffer), "%cFB:V%ld RMS:--.-",
                       selected == MENU_FB ? '>' : ' ', (long)feedback + 1L);
    else
    {
        uint32_t rms_tenths = (uint32_t)(rms * 10.0f + 0.5f);
        (void)snprintf(buffer, sizeof(buffer), "%cFB:V%ld RMS:%lu.%lu",
                       selected == MENU_FB ? '>' : ' ',
                       (long)feedback + 1L,
                       (unsigned long)(rms_tenths / 10U),
                       (unsigned long)(rms_tenths % 10U));
    }
    OLED_ShowString(0, 48, buffer, 16);
    OLED_Refresh();
}

void INV_Menu_Init(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    menu_selected = MENU_NONE;
    menu_refresh_ms = 0U;
    menu_dirty = 1U;
    menu_running = 1U;
    __set_PRIMASK(primask);
}

uint8_t INV_Menu_TakeUpdate(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint8_t dirty = menu_dirty;
    menu_dirty = 0U;
    __set_PRIMASK(primask);
    return dirty;
}

void INV_Menu_Draw(void)
{
    DrawPage();
}

/* 只在中断里调用，menu_selected 已保证不是 MENU_NONE。 */
static void MenuAdjust(int32_t step)
{
    switch (menu_selected)
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
    case MENU_FB:
        INV_SetFeedbackChannel(step > 0 ? 1 : 0);
        break;
    default:
        break;
    }
}

void INV_Menu_TickISR(void)
{
    if (!menu_running) return;

    char key = Keyboard_GetKey();

    if (key == 'D')                 /* 进入/退出参数菜单 */
    {
        if (menu_selected == MENU_NONE)
        {
            menu_selected = MENU_FREQ;
            INV_OutputDisable();
        }
        else
            menu_selected = MENU_NONE;
        menu_dirty = 1U;
    }
    else if (key == 'C')            /* 停机 */
    {
        INV_OutputDisable();
        menu_selected = MENU_NONE;
        menu_dirty = 1U;
    }
    else if (key == 'A')            /* 功率输出开关 */
    {
        if (INV_PwmFlag) INV_OutputDisable();
        else             INV_OutputEnable();
        menu_dirty = 1U;
    }
    else if ((key == '2' || key == '8') && menu_selected != MENU_NONE)
    {
        menu_selected = key == '8' ? menu_selected + 1
                                   : menu_selected + MENU_COUNT - 1;
        menu_selected %= MENU_COUNT;
        menu_dirty = 1U;
    }
    else if ((key == '4' || key == '6') && menu_selected != MENU_NONE)
    {
        MenuAdjust(key == '6' ? 1 : -1);
        menu_dirty = 1U;
    }

    /* 浏览态也要定时刷新 RMS/输出状态。 */
    if (++menu_refresh_ms >= 200U)
    {
        menu_refresh_ms = 0U;
        menu_dirty = 1U;
    }
}
