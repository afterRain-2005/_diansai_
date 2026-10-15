#include "Bipolar_PWM.h"
#include "inverter_math.h"
#include "inverter_control_tools.h"
#include "Keyboard.h"
#include <limits.h>

/* 名称约定：INV 为逆变器，PWM 为脉宽调制，ADC 为模数转换，
 * RMS 为有效值，PI 为比例积分；tick 表示 HAL 毫秒时间戳。
 * 幅度 INV_Amp 以百分数表示，active_amplitude 以千分之一百分点表示。
 */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim5;
extern ADC_HandleTypeDef hadc1;

/* 对外共享状态：由主循环、菜单和中断读取或更新。 */
volatile uint16_t INV_ADC_Results[INV_ADC_CHANNELS]; /* 各 ADC 通道最近一帧的原始码值。 */
volatile int32_t INV_Freq = INV_FREQ_DEFAULT;
volatile int32_t INV_Amp = INV_AMP_DEFAULT;
volatile int32_t INV_Vrms;
volatile int32_t INV_AdcVrms;
volatile int32_t INV_AdcCh[INV_ADC_CHANNELS];
volatile int32_t INV_PwmFlag;
volatile int32_t INV_BreakFlag;
volatile uint32_t INV_FaultFlags;

/* 内部状态说明：
 * htim1 / htim5 / hadc1：TIM1 PWM、TIM5 调制节拍、ADC1 的 HAL 句柄。
 * INV_Freq：输出基波频率，单位 Hz；INV_Amp：目标调制幅度，单位 %。
 * INV_Vrms：按参考电压及增益换算的有效值，单位 0.1 V；标定准确性另行验证。
 * INV_AdcVrms：第 0 通道的有效值，单位为 ADC 码值；INV_AdcCh：各通道原始码值。
 * INV_PwmFlag：软件输出使能状态；INV_BreakFlag：刹车故障锁存标志。
 * INV_FaultFlags：按位组合的故障原因，位定义见头文件 INV_FAULT_*。
 * sin_data：以半个 PWM 周期计数为峰值的有符号正弦表，重复两遍便于相移索引。
 * adc_dma：DMA 循环缓冲区，两半各存 INV_ADC_HALF_FRAMES 帧，每帧含全部通道。
 * table_samples / table_period：建表时使用的每周期点数 / PWM 周期计数。
 * phase_index：当前 A 相查表下标；active_amplitude：软启动后的实际调制幅度。
 * minimum_compare：比较值距周期两端的最小计数裕量，包含死区及最小门极脉宽。
 * rms_sum / rms_squares：第 0 通道样本之和 / 平方和。
 * rms_count / rms_window：已累计的样本数 / 约一个基波周期所需的样本数。
 * pi_integral：PI 的误差累加值；pi_fraction：不足一个百分点的幅度修正累计量。
 * initialized：初始化完成标志；sampling_paused：采样暂停标志。
 * adc_ready：已处理有效 ADC 数据块的标志；adc_tick：最近处理数据块的时间。
 * control_tick：主循环最近服务时间；modulation_tick：最近调制更新时间。
 * rms_sequence：有效值结果更新序号；control_rms_sequence：闭环已处理的序号。
 */
static int16_t sin_data[2 * INV_SAMP_NUMBER];
static uint16_t adc_dma[2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS] __ALIGNED(4);
static int32_t table_samples;
static uint16_t table_period;
static uint32_t phase_index;
static uint32_t active_amplitude;
static uint32_t minimum_compare;
static uint64_t rms_sum;
static uint64_t rms_squares;
static uint32_t rms_count;
static uint32_t rms_window;
static float pi_integral;
static float pi_fraction;
static volatile uint32_t initialized;
static volatile uint32_t sampling_paused;
static volatile uint32_t adc_ready;
static volatile uint32_t adc_tick;
static volatile uint32_t control_tick;
static volatile uint32_t modulation_tick;
static volatile uint32_t rms_sequence;
static uint32_t control_rms_sequence;

static void ResetRms(void)
{
    /* 清空有效值累加器，并按载波频率 / 基波频率四舍五入得到 rms_window。
     * rms_count 计样本数；rms_sum、rms_squares 分别累计样本及其平方。
     */
    rms_sum = 0U;
    rms_squares = 0U;
    rms_count = 0U;
    rms_window = (INV_CARRIER_HZ + (uint32_t)INV_Freq / 2U) / (uint32_t)INV_Freq;
}

static void LatchFault(uint32_t fault)
{
    /* 立即关闭输出并锁存 fault 故障位；刹车故障还会停用刹车中断。
     * primask 保存进入前的中断屏蔽状态，结束时原样恢复。
     */
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    INV_OutputDisable();
    INV_FaultFlags |= fault;
    if ((fault & INV_FAULT_BREAK) != 0U)
    {
        INV_BreakFlag = 1;
        __HAL_TIM_DISABLE_IT(&htim1, TIM_IT_BREAK);
    }
    __set_PRIMASK(primask);
}

void INV_OutputDisable(void)
{
    /* 清除 TIM1 主输出使能 MOE，并将控制引脚置为停机电平。
     * 同时清零实际幅度与 PI 状态；不停止定时器或 ADC，也不清除故障。
     * primask 保存原中断状态；control_rms_sequence 对齐当前有效值序号。
     */
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if ((RCC->APB2ENR & RCC_APB2ENR_TIM1EN) != 0U)
        CLEAR_BIT(TIM1->BDTR, TIM_BDTR_MOE);
    if ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN) != 0U)
        GPIOA->BSRR = ((uint32_t)PWM_EN_Pin << 16U) | INV_OFFLINE_Pin;
    if ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOBEN) != 0U)
        GPIOB->BSRR = (uint32_t)RS_CTRL_Pin << 16U;
    INV_PwmFlag = 0;
    active_amplitude = 0U;
    pi_integral = 0.0f;
    pi_fraction = 0.0f;
    control_rms_sequence = rms_sequence;
    __set_PRIMASK(primask);
}

void INV_EmergencyStop(void)
{
    /* 以 INV_FAULT_CPU 原因锁存故障并停机；具体关断由 LatchFault 完成。 */
    LatchFault(INV_FAULT_CPU);
}

int32_t INV_GetMaxAmplitude(void)
{
    /* 按 minimum_compare 两端裕量计算允许的最大调制百分数 maximum，
     * 再取其与 INV_AMP_MAX 的较小值；裕量占满周期时返回 0。
     */
    int32_t maximum;

    if (minimum_compare * 2U >= INV_PERIOD) return 0;
    maximum = (int32_t)((INV_PERIOD - 2U * minimum_compare) * 100U / INV_PERIOD);
    return maximum < INV_AMP_MAX ? maximum : INV_AMP_MAX;
}

void INV_SetAmplitude(int32_t amplitude)
{
    /* amplitude 为目标幅度百分数；maximum 为当前死区和脉宽裕量允许的上限。
     * 限幅后写入 INV_Amp；实际幅度 active_amplitude 在调制中断中更新。
     */
    int32_t maximum = INV_GetMaxAmplitude();

    if (amplitude < INV_AMP_MIN) amplitude = INV_AMP_MIN;
    if (amplitude > maximum) amplitude = maximum;
    INV_Amp = amplitude;
}

void sin_duty_init(uint16_t timer_period, int32_t samp_num)
{
    /* 停机时建立正弦表：timer_period 为 PWM 周期计数，samp_num 为一周期点数，
     * 两者必须等于工程配置。sample_index 为表下标；第二遍复制用于跨周期相移。
     * sin_data 存有符号的比较值偏移，尚未乘以目标幅度，也未加周期中心值。
     */
    int32_t sample_index;

    if (INV_PwmFlag || timer_period != INV_PERIOD || samp_num != INV_SAMP_NUMBER)
    {
        LatchFault(INV_FAULT_INIT);
        return;
    }
    for (sample_index = 0; sample_index < samp_num; sample_index++)
        sin_data[sample_index] = (int16_t)((float)timer_period * 0.5f *
            sinf(6.28318530718f * (float)sample_index / (float)samp_num));
    for (sample_index = samp_num; sample_index < 2 * INV_SAMP_NUMBER; sample_index++)
        sin_data[sample_index] = sin_data[sample_index - samp_num];
    table_samples = samp_num;
    table_period = timer_period;
    phase_index = 0U;
}

static uint32_t DutyAt(uint32_t index, uint32_t period)
{
    /* 按正弦表下标 index 计算比较值 duty（定时器计数，非百分数）。
     * period 为 PWM 周期计数；中心值为 period/2，偏移由实际幅度缩放。
     * active_amplitude=100000 表示 100%；返回值限制在两端裕量以内。
     */
    int32_t duty = (int32_t)period / 2 +
        (int32_t)sin_data[index] * (int32_t)active_amplitude / 100000;

    if (duty < (int32_t)minimum_compare) duty = (int32_t)minimum_compare;
    if (duty > (int32_t)(period - minimum_compare)) duty = (int32_t)(period - minimum_compare);
    return (uint32_t)duty;
}

void Bipolar_pwm(uint16_t pwm_period, int32_t samp_num)
{
    /* 更新三相 CCR 比较值并推进一个查表点。pwm_period / samp_num 须与建表值一致。
     * duty_a、duty_b、duty_c 是三相计数值，索引依次偏移 0、1/3、2/3 周期。
     * update_disabled 保存原 UDIS 位；写三相时暂禁更新事件，再恢复原状态。
     * primask 保存原中断状态；phase_index 到达一周期点数后回到 0。
     */
    uint32_t duty_a, duty_b, duty_c;
    uint32_t update_disabled;
    uint32_t primask;

    if (pwm_period != table_period || samp_num != table_samples || samp_num < 3)
    {
        LatchFault(INV_FAULT_INIT);
        return;
    }
    duty_a = DutyAt(phase_index, pwm_period);
    duty_b = DutyAt(phase_index + (uint32_t)samp_num / 3U, pwm_period);
    duty_c = DutyAt(phase_index + (uint32_t)samp_num * 2U / 3U, pwm_period);
    primask = __get_PRIMASK();
    __disable_irq();
    update_disabled = TIM1->CR1 & TIM_CR1_UDIS;
    SET_BIT(TIM1->CR1, TIM_CR1_UDIS);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_a);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty_b);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty_c);
    if (update_disabled == 0U) CLEAR_BIT(TIM1->CR1, TIM_CR1_UDIS);
    __set_PRIMASK(primask);
    phase_index++;
    if (phase_index >= (uint32_t)samp_num) phase_index = 0U;
}

void INV_SetFrequency(int32_t freq)
{
    /* freq 为目标基波频率（Hz），先限幅；rate 为每秒查表更新次数（点数 × Hz）。
     * 按 rate 重设 TIM5 自动重装值，并重置有效值窗口。
     * primask 用于在更新共享频率及累加器期间保存、恢复中断状态。
     */
    uint32_t primask = __get_PRIMASK();
    uint32_t rate;

    if (freq < INV_FREQ_MIN) freq = INV_FREQ_MIN;
    if (freq > INV_FREQ_MAX) freq = INV_FREQ_MAX;
    rate = (uint32_t)INV_SAMP_NUMBER * (uint32_t)freq;
    __disable_irq();
    INV_Freq = freq;
    if (htim5.Instance != NULL)
        __HAL_TIM_SET_AUTORELOAD(&htim5, (INV_TIM_CLK + rate / 2U) / rate - 1U);
    ResetRms();
    __set_PRIMASK(primask);
}

HAL_StatusTypeDef INV_SetDeadTime(int32_t deadtime_ns)
{
    /* 停机时设置 deadtime_ns（纳秒），code 为编码后的 BDTR.DTG 寄存器值。
     * margin 为死区计数加最小门极脉宽计数，作为 minimum_compare 的新值。
     * primask 保存原中断状态；参数不可用返回 HAL_ERROR，输出开启返回 HAL_BUSY，
     * 写入成功返回 HAL_OK，并按新裕量重新限制目标幅度。
     */
    uint32_t code;
    uint32_t margin;
    uint32_t primask = __get_PRIMASK();

    if (!INV_EncodeDeadTime(deadtime_ns, INV_TIM_CLK, &code)) return HAL_ERROR;
    margin = INV_DeadTimeTicks(code) +
        (INV_MIN_GATE_PULSE_NS * (INV_TIM_CLK / 1000000U) + 999U) / 1000U;
    if (margin * 2U >= INV_PERIOD || htim1.Instance == NULL) return HAL_ERROR;
    __disable_irq();
    if (INV_PwmFlag || (TIM1->BDTR & TIM_BDTR_MOE) != 0U)
    {
        __set_PRIMASK(primask);
        return HAL_BUSY;
    }
    MODIFY_REG(TIM1->BDTR, TIM_BDTR_DTG, code);
    minimum_compare = margin;
    INV_SetAmplitude(INV_Amp);
    __set_PRIMASK(primask);
    return HAL_OK;
}

HAL_StatusTypeDef INV_ClearFault(void)
{
    /* 停机且采样、调制、定时器和 BKIN 条件满足时，清除刹车/超时故障。
     * 其他故障位存在时不清除；本函数不自动开启输出。
     * status 默认为 HAL_ERROR，成功改为 HAL_OK；primask 保存原中断状态。
     */
    uint32_t primask = __get_PRIMASK();
    HAL_StatusTypeDef status = HAL_ERROR;

    __disable_irq();
    if (initialized && !INV_PwmFlag && adc_ready && !sampling_paused &&
        HAL_GetTick() - adc_tick <= INV_ADC_TIMEOUT_MS &&
        HAL_GetTick() - modulation_tick <= INV_ADC_TIMEOUT_MS &&
        (TIM1->CR1 & TIM_CR1_CEN) != 0U &&
        (TIM5->CR1 & TIM_CR1_CEN) != 0U && (TIM5->DIER & TIM_IT_UPDATE) != 0U &&
        HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_SET &&
        (INV_FaultFlags & ~(INV_FAULT_BREAK | INV_FAULT_TIMEOUT)) == 0U)
    {
        __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
        INV_FaultFlags = 0U;
        INV_BreakFlag = 0;
        __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);
        status = HAL_OK;
    }
    __set_PRIMASK(primask);
    return status;
}

HAL_StatusTypeDef INV_OutputEnable(void)
{
    /* 检查初始化、采样新鲜度、调制节拍、定时器及故障条件后开启输出。
     * 从零实际幅度和初始相位起步，UG 将预装比较值装入，再使能 MOE 和控制脚。
     * status 为操作结果；primask 保存原中断状态；后续由调制中断执行软启动。
     */
    uint32_t primask = __get_PRIMASK();
    HAL_StatusTypeDef status = HAL_ERROR;

    __disable_irq();
    if (initialized && !sampling_paused && adc_ready && INV_FaultFlags == 0U &&
        HAL_GetTick() - adc_tick <= INV_ADC_TIMEOUT_MS &&
        HAL_GetTick() - modulation_tick <= INV_ADC_TIMEOUT_MS &&
        (TIM1->CR1 & TIM_CR1_CEN) != 0U &&
        (TIM5->CR1 & TIM_CR1_CEN) != 0U && (TIM5->DIER & TIM_IT_UPDATE) != 0U &&
        HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_SET &&
        __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK) == RESET)
    {
        active_amplitude = 0U;
        pi_integral = 0.0f;
        pi_fraction = 0.0f;
        phase_index = 0U;
        Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
        TIM1->EGR = TIM_EGR_UG;
        control_tick = HAL_GetTick();
        modulation_tick = control_tick;
        __HAL_TIM_MOE_ENABLE(&htim1);
        if ((TIM1->BDTR & TIM_BDTR_MOE) != 0U &&
            __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK) == RESET)
        {
            HAL_GPIO_WritePin(INV_OFFLINE_GPIO_Port, INV_OFFLINE_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RS_CTRL_GPIO_Port, RS_CTRL_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PWM_EN_GPIO_Port, PWM_EN_Pin, GPIO_PIN_SET);
            INV_PwmFlag = 1;
            status = HAL_OK;
        }
        else LatchFault(INV_FAULT_BREAK);
    }
    __set_PRIMASK(primask);
    return status;
}

HAL_StatusTypeDef INV_SamplingPause(void)
{
    /* 先关输出，再标记采样暂停、清除 adc_ready，并停用 TIM5 更新中断及 ADC DMA。
     * status 为 HAL_ADC_Stop_DMA 的返回值；失败时锁存 ADC 故障。
     * 定时器计数器本身未在这里停止。
     */
    HAL_StatusTypeDef status;

    INV_OutputDisable();
    sampling_paused = 1U;
    adc_ready = 0U;
    __HAL_TIM_DISABLE_IT(&htim5, TIM_IT_UPDATE);
    status = HAL_ADC_Stop_DMA(&hadc1);
    if (status != HAL_OK) LatchFault(INV_FAULT_ADC);
    return status;
}

HAL_StatusTypeDef INV_SamplingResume(void)
{
    /* 重置有效值统计并重新启动 adc_dma 缓冲区采集，成功后恢复 TIM5 更新中断。
     * status 为 DMA 启动结果；adc_tick 先置当前时间，adc_ready 等数据处理后置位。
     * sampling_paused 清零表示恢复采样，本函数不恢复功率输出。
     */
    HAL_StatusTypeDef status;

    ResetRms();
    adc_ready = 0U;
    adc_tick = HAL_GetTick();
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma,
                              2U * INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS);
    if (status != HAL_OK)
    {
        LatchFault(INV_FAULT_ADC);
        return status;
    }
    __HAL_TIM_CLEAR_FLAG(&htim5, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE);
    sampling_paused = 0U;
    return HAL_OK;
}

HAL_StatusTypeDef INV_Init(void)
{
    /* 保持输出关闭，检查外设与时钟配置，建立正弦表、配置死区/频率、校准 ADC，
     * 再启动 PWM 定时器、ADC DMA 和调制中断。initialized 表示初始化流程完成。
     * init_error 为公共失败出口，锁存初始化故障并返回 HAL_ERROR。
     * 返回 HAL_OK 表示流程完成，仍需查看故障状态（如 BKIN）并显式使能输出。
     */
    INV_OutputDisable();
    initialized = 0U;
    if (INV_FaultFlags != 0U || htim1.Instance != TIM1 || htim5.Instance != TIM5 ||
        hadc1.Instance != ADC1 || SystemCoreClock != INV_TIM_CLK ||
        TIM1->PSC != 0U || TIM1->ARR != INV_PERIOD - 1U ||
        (TIM1->CR1 & (TIM_CR1_CKD | TIM_CR1_CMS | TIM_CR1_DIR)) != 0U) goto init_error;
    sin_duty_init(INV_PERIOD, INV_SAMP_NUMBER);
    if (INV_SetDeadTime(INV_DEAD_TIME_NS) != HAL_OK) goto init_error;
    INV_SetFrequency(INV_Freq);
    Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
    TIM1->EGR = TIM_EGR_UG;
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) goto init_error;
    SET_BIT(TIM1->CCER, TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E |
                       TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE);
    if (HAL_TIM_Base_Start(&htim1) != HAL_OK) goto init_error;
    if (INV_SamplingResume() != HAL_OK) goto init_error;
    if (HAL_TIM_Base_Start_IT(&htim5) != HAL_OK) goto init_error;
    initialized = 1U;
    control_tick = HAL_GetTick();
    modulation_tick = control_tick;
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_BREAK);
    if (HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_RESET)
        LatchFault(INV_FAULT_BREAK);
    else __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);
    INV_OutputDisable();
    return HAL_OK;

init_error:
    LatchFault(INV_FAULT_INIT);
    return HAL_ERROR;
}

void INV_Service(void)
{
    /* 刷新主循环服务时间 control_tick（毫秒），供安全节拍检测主循环是否超时。 */
    control_tick = HAL_GetTick();
}

void INV_SafetyTick(void)
{
    /* 处理键盘停机请求；输出开启时检查刹车及 ADC、调制、主循环服务超时。
     * now 为当前毫秒时间，与 adc_tick、modulation_tick、control_tick 作差。
     * 键盘停机仅关闭输出；检测到刹车或超时则同时锁存对应故障。
     */
    uint32_t now = HAL_GetTick();

    if (Keyboard_TakeStopRequest()) INV_OutputDisable();
    if (!initialized || !INV_PwmFlag) return;
    if (HAL_GPIO_ReadPin(INV_BKIN_GPIO_Port, INV_BKIN_Pin) == GPIO_PIN_RESET ||
        __HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_BREAK) != RESET ||
        (TIM1->BDTR & TIM_BDTR_MOE) == 0U) LatchFault(INV_FAULT_BREAK);
    else if (now - adc_tick > INV_ADC_TIMEOUT_MS ||
             now - modulation_tick > INV_ADC_TIMEOUT_MS ||
             now - control_tick > INV_CONTROL_TIMEOUT_MS) LatchFault(INV_FAULT_TIMEOUT);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* HAL 定时器周期回调，仅处理 htim 指向的 TIM5。
     * target 为目标幅度 ×1000；step 为每次调制中断的软启动递增量，单位相同。
     * 增幅按 step 渐进，降幅直接跟随目标；每次调用 Bipolar_pwm 更新三相比较值。
     * rms_sequence 与 control_rms_sequence 避免同一有效值结果重复用于闭环。
     * 闭环启用且配置为第 0 通道时，使用原始 ADC 有效值执行 Vout_Ctr。
     */
    if (htim->Instance == TIM5)
    {
        if (INV_Freq < INV_FREQ_MIN || INV_Freq > INV_FREQ_MAX)
        {
            LatchFault(INV_FAULT_INIT);
            return;
        }
        INV_SetAmplitude(INV_Amp);
        uint32_t target = (uint32_t)INV_Amp * 1000U;
        uint32_t step = 100000000U / ((uint32_t)INV_SAMP_NUMBER *
            (uint32_t)INV_Freq * INV_SOFT_START_MS) + 1U;

        modulation_tick = HAL_GetTick();
        if (!INV_PwmFlag) active_amplitude = 0U;
        else if (active_amplitude > target) active_amplitude = target;
        else if (target - active_amplitude > step) active_amplitude += step;
        else active_amplitude = target;
        Bipolar_pwm(INV_PERIOD, INV_SAMP_NUMBER);
#if INV_CLOSED_LOOP_ENABLE
        if (INV_CLOSED_LOOP_ADC_CHANNEL == 0U &&
            rms_sequence != control_rms_sequence && INV_AdcVrms <= 4095)
        {
            control_rms_sequence = rms_sequence;
            Vout_Ctr((uint16_t)INV_AdcVrms,
                     INV_CLOSED_LOOP_TARGET_ADC_RMS,
                     INV_CLOSED_LOOP_KP,
                     INV_CLOSED_LOOP_KI);
        }
#endif
    }
}

static void ProcessAdcBlock(uint32_t start)
{
    /* 处理 DMA 的一个半缓冲区；start 是起始元素下标，不是字节偏移。
     * frame_index 为帧下标，channel_index 为帧内通道下标；offset 为帧起始元素。
     * sample 取该帧第 0 通道码值；其他通道仅更新最近值，不在此计算有效值。
     * rms 为当前窗口的有效值（ADC 码值），是否去直流由 INV_RMS_REMOVE_DC 决定。
     * 窗口满后更新有效值和 rms_sequence；块处理完更新 adc_tick、adc_ready。
     */
    uint32_t frame_index, channel_index;

    for (frame_index = 0U; frame_index < INV_ADC_HALF_FRAMES; frame_index++)
    {
        uint32_t offset = start + frame_index * INV_ADC_CHANNELS;
        uint32_t sample = adc_dma[offset];

        for (channel_index = 0U; channel_index < INV_ADC_CHANNELS; channel_index++)
        {
            INV_ADC_Results[channel_index] = adc_dma[offset + channel_index];
            INV_AdcCh[channel_index] = adc_dma[offset + channel_index];
        }
        rms_sum += sample;
        rms_squares += (uint64_t)sample * sample;
        rms_count++;
        if (rms_count >= rms_window)
        {
            float rms = INV_RmsFromSums(rms_squares, rms_sum, rms_count, INV_RMS_REMOVE_DC);
            INV_AdcVrms = (int32_t)(rms + 0.5f);
            INV_Vrms = (int32_t)(rms * INV_ADC_REFERENCE_V * INV_VOLTAGE_GAIN * 10.0f / 4095.0f + 0.5f);
            rms_sequence++;
            ResetRms();
        }
    }
    adc_tick = HAL_GetTick();
    adc_ready = 1U;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    /* hadc 为触发回调的 ADC 句柄；ADC1 且未暂停时处理 DMA 前半区（起始下标 0）。 */
    if (hadc->Instance == ADC1 && !sampling_paused) ProcessAdcBlock(0U);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    /* hadc 为触发回调的 ADC 句柄；ADC1 且未暂停时处理 DMA 后半区。
     * 后半区起点为半区帧数 × 每帧通道数，单位为 uint16_t 元素。
     */
    if (hadc->Instance == ADC1 && !sampling_paused)
        ProcessAdcBlock(INV_ADC_HALF_FRAMES * INV_ADC_CHANNELS);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    /* hadc 为出错的 ADC 句柄；仅处理 ADC1，停用溢出中断 OVR 并锁存 ADC 故障。 */
    if (hadc->Instance == ADC1)
    {
        __HAL_ADC_DISABLE_IT(hadc, ADC_IT_OVR);
        LatchFault(INV_FAULT_ADC);
    }
}

void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim)
{
    /* htim 为触发刹车回调的定时器句柄；仅为 TIM1 锁存刹车故障并关闭输出。 */
    if (htim->Instance == TIM1) LatchFault(INV_FAULT_BREAK);
}

static float PiStep(uint16_t current, uint16_t target, float kp, float ki)
{
    /* 执行一次离散 PI：current 为反馈值，target 为目标值，两者须使用相同量纲。
     * kp / ki 为比例 / 积分系数；error=目标-反馈；pi_integral 累加误差并限幅。
     * output=(kp*error+ki*pi_integral)/1000，不在此显式乘采样时间。
     * 返回浮点修正量；本函数与 PI_Calculate、Vout_Ctr 共用同一积分状态。
     */
    float error = (float)target - current;
    float output;

    if (!isfinite(kp) || !isfinite(ki) || kp < 0.0f || ki < 0.0f) return 0.0f;
    pi_integral += error;
    if (pi_integral > 3000.0f) pi_integral = 3000.0f;
    if (pi_integral < -3000.0f) pi_integral = -3000.0f;
    output = (kp * error + ki * pi_integral) / 1000.0f;
    if (!isfinite(output)) return 0.0f;
    return output;
}

int32_t PI_Calculate(uint16_t cur_value, uint16_t set_value, float kp, float ki)
{
    /* cur_value 为反馈值，set_value 为设定值，kp / ki 为 PI 系数。
     * primask 保存原中断状态；output 为 PiStep 的浮点结果。
     * 结果先按 int32_t 范围饱和，再转为整数（向零截断）；不直接修改幅度。
     */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    float output = PiStep(cur_value, set_value, kp, ki);
    __set_PRIMASK(primask);

    if (output >= 2147483520.0f) return INT32_MAX;
    if (output <= -2147483648.0f) return INT32_MIN;
    return (int32_t)output;
}

void Vout_Ctr(uint16_t cur_v, uint16_t set_v, float kp, float ki)
{
    /* 按电压反馈调整目标幅度；cur_v / set_v 为同量纲反馈 / 设定值，
     * 当前闭环调用传入的是 ADC 有效值码值，并非直接以伏特计。
     * kp / ki 为 PI 系数；change 是每次限制在 ±1 个百分点内的幅度修正量。
     * pi_fraction 累计小数修正；step 为累计量的整数部分，next_amplitude 为新目标。
     * 新目标受闭环上限及 INV_SetAmplitude 限制；primask 保存原中断状态。
     */
    float change;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (!INV_PwmFlag || INV_FaultFlags != 0U)
    {
        __set_PRIMASK(primask);
        return;
    }
    change = PiStep(cur_v, set_v, kp, ki);
    if (change > 1.0f) change = 1.0f;
    if (change < -1.0f) change = -1.0f;
    pi_fraction += change;
    if (pi_fraction >= 1.0f || pi_fraction <= -1.0f)
    {
        int32_t step = (int32_t)pi_fraction;
        {
            int32_t next_amplitude = INV_Amp + step;
            if (next_amplitude > INV_CLOSED_LOOP_MAX_AMP)
                next_amplitude = INV_CLOSED_LOOP_MAX_AMP;
            INV_SetAmplitude(next_amplitude);
        }
        pi_fraction -= (float)step;
    }
    __set_PRIMASK(primask);
}
