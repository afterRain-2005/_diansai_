/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "stdio.h"
#include <stdbool.h>
#include "arm_math.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../key&oled/key.h"
#include "../../key&oled/oled.h"
#include "../../Tool_Box/Flash_Tools.h"
#include "../../Tool_Box/Calculate_Tools.h"
#include"../../Tool_Box/ADC_Tools.h"
#include"../../Tool_Box/Control_Tools.h"
#include "../../Tool_Box/Svpwm_Tools.h"
#include"../../Tool_Box/UI_Tools.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */


/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

#define CLK_FREQ     20000.0f  // 中断频率和PWM开关频率
#define SQRT2        1.414213562373f
#define Voltage_Freq 50
#define T            1/ CLK_FREQ
#define SAMP_POINT   400       // 20khz的中断，对应50hz的频率，一个周期400个点，计算有效值和偏置有用
#define W0           0.2f* PI  // 这是PR的带宽，一般这个值就够了
#define Wc           50* 2* PI // 这是PR对应的中心频率，这道题中我要去并联50hz的电网，所以用的50* 2
#define WL           (PI* 0.4f)// 这是DQ解耦对应的WL那一项的数据，电感是几mh就乘上0.几，这个是对应的4mh的数据
#define ADC_BUF_LEN  8         // ADC对应的DMA通道数字，需要几个就开几个，我这个是模板工程，配置了8个ADC通道，足够使用，如果用不到这么多记得要在ioc里面删减，并修改这个数字
#define CH_COUNT     10        // 这个是是VOFA上位机对应的通道数，一般VOFA主要用于传输一些控制的变量，可以通过vofa来调控各种控制器的参数
// vofa传输的专用结构体，不要改！！！！！
struct frame{
	float fdata[CH_COUNT];
	unsigned char tail[4];
};
// vofa传输的专用数据尾，不要改！！！！！
struct frame dat={
		.fdata={0},
		.tail={0x00,0x00,0x80,0x7f}
};
// 采样和锁相的代码，锁相这里使用的是T/4锁相，可以直接使用，具体解释看代码中的注释
// Samp & PLL
uint16_t value[ADC_BUF_LEN];//采样用的dma
extern ADC_HandleTypeDef hadc1;//这里为大家配置好了ADC1，启动DMA需要用到这行代码
extern float Uin_rms, Iin_rms, Uout_rms;//采样对应的结果有效值，适用于数据拟合，这个可以在UI_Tools里面配置
extern float U_set, PF, I_set;//这里是设置的一些参数，这个可以在UI_Tools里面配置
float Urms, Irms, Uin, Iin;//有效值计算的中间量
float Umean, Imean;//偏置计算量
float Ume, Um, Ime, Im;//偏置计算中间量
float U_error;//检测误差，用于拟合数据
SampQueue Iin_Queue = {{0},0,0};//队列，用于对输入电流I进行DQ解耦
SampQueue Uin_Queue = {{0},0,0};//队列，用于对电网电压U进行锁相
float Ualpha, Ubeta, Ud, Uq;//锁相中间量，alpha轴和beta轴要进行坐标变换来锁相
float Ialpha, Ibeta, Id, Iq;//解耦中间量，alpha轴和beta轴要进行坐标变换来进行对电流的有功无功解耦
float Udc, Id_ref, Iq_ref, U_dc, Udc_sum;//这里是用于对输出电压进行计算以及有功电流Id和无功电流Iq进行控制
float theta;//记录调制波的相位角度
float Iref;//电流设计的参考值


// PI_Controller
PI_Controller I_loop_d, I_loop_q, Udc_loop;//这里我把PI控制器整合了一下，把增量式的PI和位置式的PI都放在了PI_Controller里面，可以根据需要调用不同函数使用
PLL_Controller PLL;//锁相
PR_Controller PR_I;//PR



// inv transform//反变换计算量，可以去反推DQ解耦的值大概多大，来确定位置式PI的上下限
float UI_d, UI_q, cos_theta;
float UI_alpha, UI_beta;
float Duty1, Duty2;
float wave_d=0, wave_q=0;
float phi=0;



// key & clock & mode
extern uint8_t mode;//控制状态
extern bool PLL_done;//是否完成锁相的标志
volatile bool flag_keypad = false;
int clock_timer = 0;//时钟周期计时
int open_timer = 0; //逆变自动开关
int pid_timer = 0;  //pid计时
int n = 0;//计量定时器周期
bool Spwm_on = false;//spwm波发送开关
/* USER CODE END PV */
/**
  * @brief  The application entry point.
  * @retval int
  */
void init(void);
void vofa_send(void);
void SystemClock_Config(void);


int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */

	HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */
  MX_DMA_Init();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DAC_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_SPI3_Init();
  MX_TIM3_Init();
  MX_TIM8_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  init();
  OLED_Init();
  OLED_ColorTurn(0);
  OLED_DisplayTurn(0);
  OLED_Clear();
  // ADC的DMA通道开启
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)value, ADC_BUF_LEN);

  // HAL_Delay(4000);//休眠约3-4s
  while(1)
  {
      show();// 屏幕显示函数
      vofa_send();// vofa发送函数，想要vofa发送的波形好看一些，需要把屏幕显示注释掉，或者单开一个中断用于显示
	  if (flag_keypad)// 按键检测
	  {
		  flag_keypad = false;
		  keypad();
	  }
  }
  /* USER CODE END 3 */
}



/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == htim3.Instance) //20kHz
    {
		clock_timer = (clock_timer + 1) % (int)CLK_FREQ;
		n = clock_timer % SAMP_POINT;
//---------------------------------数据拟合-----------------------------------------
    	Udc = value[0] * 3.3f / 4095 * 23.09f + 0.33f;
    	Um = (value[1] * 3.3f / 4095);
        Uin = (value[1] * 3.3f / 4095 - Umean) * 33.85428f - 0.12788f;
        Im = (value[2] * 3.3f / 4095);
        Iin = -(value[2] * 3.3f / 4095 - Imean) * 2.6069f - 0.0f;
    	Urms += Uin * Uin;
    	Irms += Iin * Iin;
        Ume += Um;
        Ime += Im;
        Udc_sum += Udc;
//---------------------------------队列延时计算PLL和电流解耦-----------------------------------------
        enQueue(&Uin_Queue, Uin);
        enQueue(&Iin_Queue, Iin);
	    // T/4延时法计算Ualpha和Ubeta  Ialpha和Ibeta
	    Ualpha = Uin;
	    Ubeta = Uin_Queue.data[(Uin_Queue.rear- 100+ maxsize)% maxsize];
	    Ialpha = Iin;
	    Ibeta = Iin_Queue.data[(Iin_Queue.rear- 100+ maxsize)% maxsize];
	    // PLL锁相环
	    PLL_Update(&PLL, Ualpha, Ubeta);//锁相出来的角度是PLL.ws，对应的sin和cos则是PLL.sin_theta, PLL.cos_theta
	    arm_park_f32(Ialpha, Ibeta, &Id, &Iq, PLL.sin_theta, PLL.cos_theta);
//-----------------------------------------------模式切换-------------------------------------------------
	    if(mode == 2){
	    	//-----恒流输入----（整流2A输入，PF为1）
	        Id_ref = I_set * SQRT2;//设定电流为Iset，最大值为Iset*sqrt2
	        Iq_ref = 0.0f;//控制无功电流为0，即PF为1
	        theta = PLL.ws;//PR要跟踪的相位
		}
	    else if(mode == 3){
	    	//-----PF设定----（整流2A输入的情况下，修改PF）
	        Id_ref = I_set * SQRT2;//根据PF计算Id
	        Iq_ref = I_set * SQRT2 * sqrtf(1 - PF * PF);//根据PF计算Iq
	        phi = acosf(PF);//用要设定的PF反推相位角
	        theta = PLL.ws - phi;//PR要跟踪的相位
	    }
	    else if(mode == 1){
	    	//-----恒压输出----（设定输出电压恒定，且PF为1）
	        float voltage_error = U_set - U_dc;
	        PI_Pos_Cal(&Udc_loop, voltage_error);//位置式PI进行电压外环的计算
	        Id_ref = fix(Udc_loop.Pos, 0.5f, 3.5f);//电压外环的输出限幅
	        Iq_ref = 0.0f;//控制无功电流为0，即PF为1
	        theta = PLL.ws;//PR要跟踪的相位
	    }
	    if(PLL_done && Spwm_on){//锁相成功并且发波才进行下列运算
	    	//电流内环
	    	//这一段是DQ解耦用的代码，使用前需要看一下，我记得mode2肯定是可以用的，但可能改了前面之后现在有问题，如果有无法解决的问题可以联系我
//			PI_Pos_Cal(&I_loop_d, Id_ref - Id);
//			PI_Pos_Cal(&I_loop_q, Iq_ref - Iq);
//	    	UI_d = (-I_loop_d.Pos + PLL.ud - Iq * WL) / 50.0f;
//	    	UI_q = (-I_loop_q.Pos + PLL.uq + Id * WL) / 50.0f;
//	        //inv park
//	        arm_inv_park_f32(UI_d, UI_q, &UI_alpha, &UI_beta, PLL.sin_theta, PLL.cos_theta);

	    	cos_theta = arm_cos_f32(theta) * 0.99f;//计算PR需要跟踪的波形
	    	PR_Cal(&PR_I, cos_theta * Id_ref - Iin, 1);//PR控制器，输入是要跟踪的波形，输出是PR.output
	    	UI_alpha = (Uin - PR_I.output) / U_dc;//60V输出的时候，控制2A最稳
	        UI_alpha = fix(UI_alpha, -0.95f, 0.95f);//这个是一个控制大小的函数，控制最小值和最大值在-0.95和+0.95之间

	        // 单极性倍频调制
	        Duty1 = (1 + UI_alpha) * 0.5f;//Duty1对应正极半桥桥臂（DQ解耦）
	        Duty2 = (1 - UI_alpha) * 0.5f;//Duty2对应负极半桥桥臂（DQ解耦）
	    	TIM1->CCR1 = (int)(TIM1->ARR+1) * Duty2;
	    	TIM1->CCR2 = (int)(TIM1->ARR+1) * Duty1;
	    }
//-------------------------------------------------DAC------------------------------------------------
        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (UI_alpha + 1.3) * 4096 / 3.3f);//DAC可以用来验证数据正确性
        HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
//-------------------------------------------------RMS-------------------------------------------------
        if(n == 0){//n每计算400个点，也就是一个周期就进行一次重置
            Umean = Ume / SAMP_POINT;
            Imean = Ime / SAMP_POINT;
            Urms = Urms / SAMP_POINT;
            Irms = Irms / SAMP_POINT;
            U_dc = Udc_sum / SAMP_POINT;
            arm_sqrt_f32(Urms, &Uin_rms);
            arm_sqrt_f32(Irms, &Iin_rms);
            Urms = 0.0f;
            Irms = 0.0f;
            Ume = 0.0f;
            Ime = 0.0f;
            Udc_sum = 0.0f;
		}
		if(clock_timer % 2000 == 0)
		{//锁相检测的代码
			if(!Spwm_on && Uin_rms > 22.5f)//大于22.5V才进行锁相
			{
				if(fabs(Uq) < 0.5f) open_timer++;
				else if(fabs(Uq) > 1.0f){
					open_timer = open_timer - 5;
					if(open_timer < 0)open_timer = 0;
				}
				if(open_timer == 50)
				{//锁相成功的标志
					PLL_done = true;
		    		open_timer = 0;
				}
			}
		}
        if(clock_timer % 4000 == 0){//按键检测标志
            flag_keypad = true;
        }
    }
}
void init(void){
    //LCD
    OLED_Init();
    OLED_ColorTurn(0);
    OLED_DisplayTurn(0);
    OLED_Clear();

    //PWM通道初始化
    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);

    PI_Pos_Init(&I_loop_d, 3.0f, 0.00033f, -100.0f, 100.0f);//这一段初始化就位置式的PI初始化
    PI_Pos_Init(&I_loop_q, 3.0f, 0.00033f, -100.0f, 100.0f);//这一段初始化就位置式的PI初始化
    PR_Init(&PR_I, T, 2, 25, W0, Wc, 600);//PR初始化

    PI_Pos_Init(&Udc_loop, 0.00005f, 0.00008f, -4.0f, 4.0f);//这一段初始化就位置式的PI初始化
    PLL_Init(&PLL, 60.2f, 0.006f, CLK_FREQ, Voltage_Freq);//锁相环初始化

	//禁用 DMA 完成中断，防止打断主循环
	__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_TC);
	HAL_Delay(400);
    HAL_TIM_Base_Start_IT(&htim3);//主控制定时器中断使能
}
// vofa发送数据，直接在这里修改需要发送的数据，注意要是float类型的数据，并且要记得在define里面修改需要发送的数据个数
void vofa_send(void){
	dat.fdata[0]=Udc_loop.Pos;
	dat.fdata[1]=U_dc - U_set;
	dat.fdata[2]=Id;
	dat.fdata[3]=Iq;
	dat.fdata[4]=cos_theta * Id_ref;
	dat.fdata[5]=Iin;
	dat.fdata[6]=Udc;
	dat.fdata[7]=Uin;
	dat.fdata[8]=Iin;
	dat.fdata[9]=Umean;
	HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&dat, sizeof(struct frame));
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
