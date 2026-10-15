#include "inverter_menu.h"
#include "Bipolar_PWM.h"
#include "inverter_math.h"
#include "oled.h"
#include "Keyboard.h"
#include <string.h>
#include <stdio.h>

typedef struct
{
    const char *name;
    volatile int32_t *value;
    uint8_t decimals;
    uint8_t editable;
    int32_t minimum;
    int32_t maximum;
    void (*apply)(int32_t);
} INV_MENU_MEMBER;

static const INV_MENU_MEMBER menu[] =
{
    {"VRMS", &INV_Vrms, 1, 0, 0, 0, NULL},
    {"ADC RMS", &INV_AdcVrms, 0, 0, 0, 0, NULL},
    {"FREQ", &INV_Freq, 0, 1, INV_FREQ_MIN, INV_FREQ_MAX, INV_SetFrequency},
    {"AMP", &INV_Amp, 0, 1, INV_AMP_MIN, INV_AMP_MAX, INV_SetAmplitude},
    {"ADC0", &INV_AdcCh[0], 0, 0, 0, 0, NULL},
    {"ADC1", &INV_AdcCh[1], 0, 0, 0, 0, NULL},
    {"ADC2", &INV_AdcCh[2], 0, 0, 0, 0, NULL},
    {"ADC3", &INV_AdcCh[3], 0, 0, 0, 0, NULL},
};
#define MENU_SIZE ((int32_t)(sizeof(menu) / sizeof(menu[0])))
#define MENU_LINES 3

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    int32_t frequency;
    int32_t amplitude;
    uint32_t crc;
    uint32_t reserved[2];
} INV_PARAM_T;

static int ParamsValid(const INV_PARAM_T *param)
{
    return param->magic == INV_FLASH_MAGIC && param->version == INV_FLASH_VERSION &&
        param->frequency >= INV_FREQ_MIN && param->frequency <= INV_FREQ_MAX &&
        param->amplitude >= INV_AMP_MIN && param->amplitude <= INV_AMP_MAX &&
        param->crc == INV_Crc32(param, offsetof(INV_PARAM_T, crc));
}

static int ReadLatest(INV_PARAM_T *param)
{
    INV_PARAM_T first, second;
    int first_valid, second_valid;

    memcpy(&first, (const void *)INV_FLASH_ADDR, sizeof(first));
    memcpy(&second, (const void *)(INV_FLASH_ADDR + INV_FLASH_SLOT_SIZE), sizeof(second));
    first_valid = ParamsValid(&first);
    second_valid = ParamsValid(&second);
    if (!first_valid && !second_valid) return -1;
    if (first_valid && (!second_valid || first.sequence == second.sequence ||
        first.sequence - second.sequence < 0x80000000U))
    {
        *param = first;
        return 0;
    }
    *param = second;
    return 1;
}

static void FormatValue(char *buffer, size_t size, int32_t value, uint8_t decimals)
{
    uint32_t magnitude = value < 0 ? 0U - (uint32_t)value : (uint32_t)value;

    if (decimals == 1U)
        (void)snprintf(buffer, size, "%s%lu.%lu", value < 0 ? "-" : "",
                       (unsigned long)(magnitude / 10U), (unsigned long)(magnitude % 10U));
    else (void)snprintf(buffer, size, "%ld", (long)value);
}

void INV_Menu_Init(void)
{
    INV_PARAM_T param;

    INV_SetFrequency(INV_FREQ_DEFAULT);
    INV_SetAmplitude(INV_AMP_DEFAULT);
    if (FLASH_SIZE == 0x80000U && ReadLatest(&param) >= 0)
    {
        INV_SetFrequency(param.frequency);
        INV_SetAmplitude(param.amplitude);
    }
}

HAL_StatusTypeDef INV_Menu_Save(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    INV_PARAM_T previous, param;
    uint64_t words[sizeof(INV_PARAM_T) / sizeof(uint64_t)];
    uint32_t target, bank, page, page_size, error, index;
    int latest;
    HAL_StatusTypeDef status;
    HAL_StatusTypeDef resume_status;

    INV_OutputDisable();
    if (FLASH_SIZE != 0x80000U) return HAL_ERROR;
    latest = ReadLatest(&previous);
    param.magic = INV_FLASH_MAGIC;
    param.version = INV_FLASH_VERSION;
    param.sequence = latest < 0 ? 0U : previous.sequence + 1U;
    param.frequency = INV_Freq;
    param.amplitude = INV_Amp;
    param.reserved[0] = 0xFFFFFFFFU;
    param.reserved[1] = 0xFFFFFFFFU;
    param.crc = INV_Crc32(&param, offsetof(INV_PARAM_T, crc));
    if (!ParamsValid(&param)) return HAL_ERROR;
    if (latest >= 0 && previous.frequency == param.frequency &&
        previous.amplitude == param.amplitude) return HAL_OK;
    target = INV_FLASH_ADDR + (latest == 0 ? INV_FLASH_SLOT_SIZE : 0U);
    if (!INV_FlashPage(target, FLASH_SIZE, (FLASH->OPTR & FLASH_OPTR_DBANK) != 0U,
        (SYSCFG->MEMRMP & SYSCFG_MEMRMP_FB_MODE) != 0U, &bank, &page, &page_size))
        return HAL_ERROR;
    if (INV_SamplingPause() != HAL_OK) return HAL_ERROR;
    status = HAL_FLASH_Unlock();
    if (status == HAL_OK)
    {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_SR_ERRORS);
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Banks = bank == 1U ? FLASH_BANK_1 : FLASH_BANK_2;
        erase.Page = page;
        erase.NbPages = INV_FLASH_SLOT_SIZE / page_size;
        status = HAL_FLASHEx_Erase(&erase, &error);
        memcpy(words, &param, sizeof(words));
        for (index = 1U; index < sizeof(words) / sizeof(words[0]) && status == HAL_OK; index++)
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, target + index * 8U, words[index]);
        if (status == HAL_OK)
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, target, words[0]);
        if (status == HAL_OK && memcmp((const void *)(uintptr_t)target, &param, sizeof(param)) != 0)
            status = HAL_ERROR;
    }
    if (HAL_FLASH_Lock() != HAL_OK) status = HAL_ERROR;
    resume_status = INV_SamplingResume();
    return status == HAL_OK ? resume_status : status;
}

void INV_ShowStandby(void)
{
    char buffer[24];

    OLED_Clear();
    if (INV_FaultFlags != 0U)
        (void)snprintf(buffer, sizeof(buffer), "FAULT:%02lX B:RST", (unsigned long)INV_FaultFlags);
    else (void)snprintf(buffer, sizeof(buffer), "INV OUT:%s", INV_PwmFlag ? "ON" : "OFF");
    OLED_ShowString(0, 0, buffer, 16);
    (void)snprintf(buffer, sizeof(buffer), "F:%ldHZ A:%ld%%", (long)INV_Freq, (long)INV_Amp);
    OLED_ShowString(0, 16, buffer, 16);
    if (INV_VOLTAGE_CALIBRATED)
    {
        OLED_ShowString(0, 32, "VRMS:", 16);
        FormatValue(buffer, sizeof(buffer), INV_Vrms, 1);
        OLED_ShowString(48, 32, buffer, 16);
    }
    else
    {
        (void)snprintf(buffer, sizeof(buffer), "UNCAL RMS:%ld", (long)INV_AdcVrms);
        OLED_ShowString(0, 32, buffer, 16);
    }
    OLED_ShowString(0, 48, "A:ON C:OFF D:SET", 16);
    OLED_Refresh();
}

static void DrawMenu(int32_t selected, int32_t start, uint8_t editing, int32_t draft)
{
    char buffer[24];
    int32_t index, line;

    OLED_Clear();
    (void)snprintf(buffer, sizeof(buffer), "%s %ld/%ld", editing ? "EDIT" : "SET",
                   (long)(selected + 1), (long)MENU_SIZE);
    OLED_ShowString(0, 0, buffer, 16);
    for (index = start, line = 0; index < MENU_SIZE && line < MENU_LINES; index++, line++)
    {
        uint8_t vertical = (uint8_t)(16 + line * 16);
        OLED_ShowChar(0, vertical, index == selected ? (editing ? '#' : '>') : ' ', 16);
        OLED_ShowString(8, vertical, menu[index].name, 16);
        if (index == 0 && !INV_VOLTAGE_CALIBRATED)
            OLED_ShowString(80, vertical, "UNCAL", 16);
        else
        {
            FormatValue(buffer, sizeof(buffer), editing && index == selected ? draft :
                        *menu[index].value, menu[index].decimals);
            OLED_ShowString(80, vertical, buffer, 16);
        }
    }
    OLED_Refresh();
}

void INV_Menu_Run(void)
{
    static int32_t selected;
    int32_t start = selected < MENU_LINES ? 0 : selected - MENU_LINES + 1;
    int32_t draft = 0;
    uint8_t editing = 0U;
    uint8_t modified = 0U;
    uint32_t tick;

    INV_OutputDisable();
    DrawMenu(selected, start, editing, draft);
    tick = HAL_GetTick();
    while (1)
    {
        char key;
        INV_Service();
        key = Keyboard_GetKey();
        if (key == 'C')
        {
            INV_OutputDisable();
            return;
        }
        if (editing)
        {
            int32_t step = key == '2' ? 1 : key == '8' ? -1 :
                           key == '6' ? 10 : key == '4' ? -10 : 0;
            int32_t maximum = menu[selected].apply == INV_SetAmplitude ?
                              INV_GetMaxAmplitude() : menu[selected].maximum;
            draft += step;
            if (draft < menu[selected].minimum) draft = menu[selected].minimum;
            if (draft > maximum) draft = maximum;
            if (key == 'A')
            {
                if (draft != *menu[selected].value)
                {
                    menu[selected].apply(draft);
                    modified = 1U;
                }
                editing = 0U;
            }
            else if (key == 'D') editing = 0U;
        }
        else if (key == '2') selected = selected > 0 ? selected - 1 : MENU_SIZE - 1;
        else if (key == '8') selected = selected < MENU_SIZE - 1 ? selected + 1 : 0;
        else if (key == 'A' && menu[selected].editable)
        {
            draft = *menu[selected].value;
            editing = 1U;
        }
        else if (key == 'D')
        {
            if (!modified || INV_Menu_Save() == HAL_OK) return;
            OLED_Clear();
            OLED_ShowString(0, 0, "SAVE FAILED", 16);
            OLED_Refresh();
            HAL_Delay(750);
        }
        if (selected < start) start = selected;
        if (selected >= start + MENU_LINES) start = selected - MENU_LINES + 1;
        if (key != KEYBOARD_NO_KEY || HAL_GetTick() - tick >= 200U)
        {
            DrawMenu(selected, start, editing, draft);
            tick = HAL_GetTick();
        }
        HAL_Delay(1);
    }
}
