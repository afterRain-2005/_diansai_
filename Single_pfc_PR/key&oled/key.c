/*
 * key.c
 *
 *  Created on: Jun 25, 2025
 *      Author: HuaWenxi
 */
#include "gpio.h"

#include "../key&oled/oled.h"
//------------------------------矩阵键盘读取----------------------------------------
uint8_t get_keyboard_value(void)
{
    int h_arr[4] = {KEY_H1_Pin, KEY_H2_Pin, KEY_H3_Pin, KEY_H4_Pin};
    int v_arr[4] = {KEY_V1_Pin, KEY_V2_Pin, KEY_V3_Pin, KEY_V4_Pin};
    uint8_t board[16]={'1','2','3','A','4','5','6','B','7','8','9','C','*','0','#','D'};

    int i = 0;
		int j = 0;
    int key_value = 0;
    for (i = 0; i < 4; i++)
    {
    	HAL_GPIO_WritePin((i>0) ? GPIOE:GPIOD,v_arr[i],0);
    	HAL_GPIO_WritePin(((i+1)%4>0) ? GPIOE:GPIOD,v_arr[(i + 1) % 4],1);
    	HAL_GPIO_WritePin(((i+2)%4>0) ? GPIOE:GPIOD,v_arr[(i + 2) % 4],1);
    	HAL_GPIO_WritePin(((i+3)%4>0) ? GPIOE:GPIOD,v_arr[(i + 3) % 4],1);

        //HAL_Delay(20);

        for (j = 0; j < 4; j++)
        {
            if (HAL_GPIO_ReadPin(GPIOD, h_arr[j]) == 0)
            {
                key_value = j * 4 + i + 1;
            }
        }

    }
    if(key_value!=0)
    {
    	return board[key_value-1];
    }else{
    	return 0;
    }

}
//------------------------------user_key切换页面-------------------------------------------
uint8_t KEY_FLAG = 0;
extern int mode;
uint8_t k = 2;//显示页面有几页-----根据页面切换状态
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch(GPIO_Pin)
	{
	case USER_KEY_Pin: KEY_FLAG=(KEY_FLAG+1)%k;//按一下切换一下页面，总共有k页
	                   mode = mode % 2 +1;
	                   OLED_Clear();break;
	}
}
