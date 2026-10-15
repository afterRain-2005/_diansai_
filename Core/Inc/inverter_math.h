#ifndef INVERTER_MATH_H
#define INVERTER_MATH_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

static inline uint32_t INV_DeadTimeTicks(uint32_t code)
{
    if (code < 128U) return code;
    if (code < 192U) return (64U + (code & 63U)) * 2U;
    if (code < 224U) return (32U + (code & 31U)) * 8U;
    return (32U + (code & 31U)) * 16U;
}

static inline int INV_EncodeDeadTime(int32_t nanoseconds, uint32_t clock_hz,
                                     uint32_t *code)
{
    uint64_t required;
    uint32_t candidate;

    if (nanoseconds <= 0 || clock_hz == 0U || code == NULL) return 0;
    required = ((uint64_t)(uint32_t)nanoseconds * clock_hz + 999999999ULL) / 1000000000ULL;
    for (candidate = 1U; candidate <= 255U; candidate++)
    {
        if (INV_DeadTimeTicks(candidate) >= required)
        {
            *code = candidate;
            return 1;
        }
    }
    return 0;
}

static inline uint32_t INV_Crc32(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;
    size_t index;
    uint32_t bit;

    for (index = 0U; index < length; index++)
    {
        crc ^= bytes[index];
        for (bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
    }
    return ~crc;
}

static inline float INV_RmsFromSums(uint64_t squares, uint64_t sum,
                                   uint32_t count, int remove_dc)
{
    uint64_t numerator;
    uint64_t denominator;

    if (count == 0U) return 0.0f;
    if (!remove_dc) return sqrtf((float)squares / (float)count);
    numerator = squares * count;
    if (numerator <= sum * sum) return 0.0f;
    numerator -= sum * sum;
    denominator = (uint64_t)count * count;
    return sqrtf((float)numerator / (float)denominator);
}

static inline int INV_FlashPage(uint32_t address, uint32_t flash_size,
                               int dual_bank, int swapped,
                               uint32_t *bank, uint32_t *page,
                               uint32_t *page_size)
{
    uint32_t offset;
    uint32_t half_size = flash_size / 2U;

    if (bank == NULL || page == NULL || page_size == NULL ||
        flash_size != 0x80000U || address < 0x08000000U ||
        address >= 0x08000000U + flash_size) return 0;
    offset = address - 0x08000000U;
    *page_size = dual_bank ? 2048U : 4096U;
    *bank = 1U;
    if (dual_bank)
    {
        *bank = ((offset >= half_size) != (swapped != 0)) ? 2U : 1U;
        offset %= half_size;
    }
    *page = offset / *page_size;
    return 1;
}

#endif
