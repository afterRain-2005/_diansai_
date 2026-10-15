/*
 * Flash_Tools.c
 *
 *  Created on: Jul 9, 2025
 *      Author: HuaWenxi
 */
#include "Flash_Tools.h"
// 内部辅助函数：根据地址获取其所在的扇区
static uint32_t GetSector(uint32_t address) {
    if ((address >= 0x08000000) && (address < 0x08003FFF)) return FLASH_SECTOR_0;
    if ((address >= 0x08004000) && (address < 0x08007FFF)) return FLASH_SECTOR_1;
    if ((address >= 0x08008000) && (address < 0x0800BFFF)) return FLASH_SECTOR_2;
    if ((address >= 0x0800C000) && (address < 0x0800FFFF)) return FLASH_SECTOR_3;
    if ((address >= 0x08010000) && (address < 0x0801FFFF)) return FLASH_SECTOR_4;
    if ((address >= 0x08020000) && (address < 0x0803FFFF)) return FLASH_SECTOR_5;
    return -1;
}
/**
  * 将数据写入到Flash
  */
Flash_Status Flash_Write(uint32_t address, uint32_t *data, uint32_t num_words) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    uint32_t sector;
    // 解锁Flash
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return FLASH_ERROR;
    }
    // 擦除扇区
    sector = GetSector(address);
    if (sector == -1) {
        HAL_FLASH_Lock();
        return FLASH_ERROR;
    }
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // STM32F407使用此电压范围
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = 1; // 只擦除一个扇区
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return FLASH_ERROR;
    }
    // 写入数据
    for (uint32_t i = 0; i < num_words; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + (i * 4), data[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return FLASH_ERROR;
        }
    }
    // 锁定Flash
    HAL_FLASH_Lock();
    return FLASH_OK;
}
/**
  * 从Flash读取数据
  */
void Flash_Read(uint32_t address, uint32_t *data, uint32_t num_words) {
    for (uint32_t i = 0; i < num_words; i++) {
        // 直接通过指针读取内存
        data[i] = *(__IO uint32_t*)(address + (i * 4));
    }
}


