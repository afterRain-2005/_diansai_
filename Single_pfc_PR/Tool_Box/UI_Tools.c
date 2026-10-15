/*
 * UI_Tools.c
 *
 *  Created on: Jul 23, 2025
 *      Author: HuaWenxi
 */
#include "UI_Tools.h"
#include "main.h"
#include "gpio.h"
#include "math.h"
#include "tim.h"
#include <string.h>
#include "adc.h"
#include "arm_math.h"
#include <stdbool.h>
#include "../key&oled/key.h"
#include "../key&oled/oled.h"
extern int n;
extern float PF_Cal, Umean, Imean, U_dc;
/********************************常量定义****************************/
bool PLL_done = false;
bool read_flag = false;
bool write_flag = false;
bool administrators = false;    //管理员模式开关
extern bool Spwm_on;
uint8_t mode = 2;               //模式选择，0:等待,   1:电压源,  2:电流源
uint8_t set_mode = 0;
uint8_t dataget;                //按键数据变量
float U_set = 50.0f;
float I_set = 2.0f;
float PF = 1.0f;
float Uin_rms, Uout_rms, Iin_rms;
/********************************快速按键功能****************************/
#define parameter_tot 3                                   //按键快捷变量 个数
int parameter_num = 3;                                    //按键快捷变量 选择
char parameter_name[parameter_tot][10] = {"U_set","I_set","PF"}; //按键快捷变量 名称
float* parameter[parameter_tot] = {&U_set,&I_set,&PF};          //按键快捷变量 取地址
float parameter_little_step[parameter_tot] = {0.1, 0.01,0.01};  //按键快捷变量 小步进
float parameter_big_step[parameter_tot] = {2, 0.1,0.1};       //按键快捷变量 大步进
float parameter_lower[parameter_tot] = {40.0, 1.7,-1.0};        //按键快捷变量 变量下限
float parameter_upper[parameter_tot] = {60.0, 2.5,1.0};       //按键快捷变量 变量上限
/******************************** 功能：按键命令解析****************************/
void keypad() {
    dataget = get_keyboard_value();;
    if (!administrators) {
        if (dataget == 0) return;
        if (dataget == '1') {
        	set_mode = (set_mode + 1) % 3 ; // 切换按键状态
        }
        if (dataget == '2') {
        	mode = mode % 3 + 1; // 切换模式
        }
        if (dataget == '6') {
        	if(PLL_done){
        		Spwm_on = !Spwm_on;// spwm
				if(!Spwm_on){
					HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
					HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_1);
					HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
					HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
				}else{
					HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
					HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_1);
					HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
					HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
				}
        	}else{
        		Spwm_on = false;
        	}
        }
        //-------------------------精细调节-------------------------------
        if (dataget == '9') {
            if(*parameter[set_mode] <= (parameter_upper[set_mode]-parameter_little_step[set_mode]))
            *parameter[set_mode] += parameter_little_step[set_mode];
            else
                *parameter[set_mode] = parameter_upper[set_mode];
        }
        if (dataget == '7') {
            if(*parameter[set_mode] >= (parameter_lower[set_mode]+parameter_little_step[set_mode]))
            *parameter[set_mode] -= parameter_little_step[set_mode];
            else
                *parameter[set_mode] = parameter_lower[set_mode];
        }//-------------------------快速调节-------------------------------
        if (dataget == '#') {
            if(*parameter[set_mode] <= (parameter_upper[set_mode]-parameter_big_step[set_mode]))
            *parameter[set_mode] += parameter_big_step[set_mode];
            else
                *parameter[set_mode] = parameter_upper[set_mode];
        }
        if (dataget == '*') {
            if(*parameter[set_mode] >= (parameter_lower[set_mode]+parameter_big_step[set_mode]))
            *parameter[set_mode] -= parameter_big_step[set_mode];
            else
                *parameter[set_mode] = parameter_lower[set_mode];
        }//-------------------------输入调节-------------------------------
    }
    //******************************进入管理者模式**********************************
    if (dataget == 'C') {
        administrators = true; //管理者模式
    }
    //******************************退出管理者模式**********************************
    if (dataget == 'D') {
        administrators = false; //退出管理者模式
    }
    //******************************管理者模式写flash**********************************
    if (administrators&&(dataget == 'A')){
    	write_flag =!write_flag;
    }
    //******************************管理者模式读flash**********************************
    if (administrators&&(dataget == 'B')){
    	read_flag =!read_flag;
    }
}
/********************************LCD显示UI界面****************************/
void show(void) {
    int line = 0;
    int size = 8;
    if (set_mode==0){
		  OLED_ShowString(72,line,"SetU ",size,1);
    }
    else if (set_mode==1){
		  OLED_ShowString(72,line,"SetI ",size,1);
    }
    else if (set_mode==2){
		  OLED_ShowString(72,line,"SetPF",size,1);
    }
    if (administrators){
        OLED_ShowString(114, line, "!!",size,1);
    }else if(!administrators){
        OLED_ShowString(114, line, "  ",size,1);
    }
    OLED_ShowString(0, line, "PFC_signle",size,1);
    if(dataget!=0){
    	OLED_ShowChar(108, line, dataget,size,1);
    }
	OLED_ShowString(0,line+=size,"Uset",size,1);
	OLED_ShowFloat(30,line,U_set,2,4,size,1);
	OLED_ShowString(0,line+=size,"Iset",size,1);
	OLED_ShowFloat(30,line,I_set,2,4,size,1);
	OLED_ShowString(0,line+=size,"PF",size,1);
	OLED_ShowFloat(30,line,PF,2,2,size,1);

	OLED_ShowString(0,line+=size,"Uin",size,1);
	OLED_ShowFloat(40,line,Uin_rms,2,4,size,1);
	OLED_ShowString(0,line+=size,"Iin",size,1);
	OLED_ShowFloat(40,line,Iin_rms,2,4,size,1);
	OLED_ShowString(0,line+=size,"Uout",size,1);
	OLED_ShowFloat(40,line,U_dc,2,4,size,1);

    if(Spwm_on)OLED_ShowString(90,8,"ON!!!",size,0);
    else OLED_ShowString(90,8,"off  ",size,1);

    if(mode==1)OLED_ShowString(0,56,"U ",size,0);
    else if(mode==2)OLED_ShowString(0,56,"I ",size,0);
    else if(mode==3)OLED_ShowString(0,56,"PF",size,0);

    if(PLL_done)OLED_ShowString(30,56,"PLL_done!!",size,0);
    else OLED_ShowString(30,56,"PLL_no....",size,1);
    OLED_Refresh();
}

