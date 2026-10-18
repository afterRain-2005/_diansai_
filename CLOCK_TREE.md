# STM32G474 三相逆变器时钟树

## 当前配置

主时钟使用底板 PF0/PF1 上的 8 MHz HSE 晶振，不再使用精度和温漂较差的 16 MHz HSI 作为 PLL 基准。

```text
HSE 8 MHz
  -> PLLM /2       = 4 MHz PLL 输入
  -> PLLN x85      = 340 MHz VCO
  -> PLLR /2       = 170 MHz SYSCLK
  -> AHB /1        = 170 MHz HCLK
  -> APB1 /1       = 170 MHz PCLK1、TIM5
  -> APB2 /1       = 170 MHz PCLK2、TIM1
```

电源使用 `PWR_REGULATOR_VOLTAGE_SCALE1_BOOST`，Flash 等待周期为 4，符合 170 MHz 运行要求。

## 与逆变器的关系

| 功能 | 时钟与结果 |
| --- | --- |
| TIM1 三相 PWM | 170 MHz / 3400 = 50 kHz，ARR = 3399 |
| TIM5 调制更新 | 170 MHz / (360 × 50 Hz) ≈ 9444 个计数 |
| ADC1 | SYSCLK 170 MHz / 4 = 42.5 MHz |
| ADC 四通道序列 | 每通道 47.5 周期采样 + 12.5 周期转换，总计约 5.65 us，小于 20 us PWM 周期 |

启动时软件会检查 PLL 来源和 SYSCLK、HCLK、PCLK1、PCLK2 是否均符合 170 MHz；不符合时进入 `Error_Handler()`，保持逆变器输出关闭。

HSE 时钟安全系统 CSS 已启用。运行中若外部晶振失效，会触发 NMI，现有 `NMI_Handler()` 调用 `INV_EmergencyStop()` 并停机，防止载波、死区和控制周期在未知时钟下继续工作。

## 硬件确认

上电前确认底板 PF0-OSC_IN、PF1-OSC_OUT 连接的是 8 MHz 晶振及其匹配负载电容。若实物晶振不是 8 MHz，必须同时修改 `HSE_VALUE`、PLL 参数和 `.ioc`，不能直接使用本配置。
