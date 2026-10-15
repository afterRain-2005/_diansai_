/*
 * Flash_Tools.h
 *
 *  Created on: Jul 9, 2025
 *      Author: HuaWenxi
 */



#ifndef FLASH_TOOLS_H_
#define FLASH_TOOLS_H_

#include "main.h"

// 定义要存储数据的地址和扇区
#define FLASH_STORAGE_ADDRESS   0x08010000U // 使用Sector 4
#define FLASH_STORAGE_SECTOR    FLASH_SECTOR_4

// 函数返回值定义
typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR
} Flash_Status;

Flash_Status Flash_Write(uint32_t address, uint32_t *data, uint32_t num_words);
void Flash_Read(uint32_t address, uint32_t *data, uint32_t num_words);


#endif /* FLASH_TOOLS_H_ */
