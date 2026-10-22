# 交接说明：TIM8 迁移后的 ADC 触发修复

> 用途：把这份文件交给另一个工作副本里的 AI 执行。它是自包含的，不需要原对话上下文。
> 目标仓库：STM32G474VETx 三相 SPWM 逆变器固件（Keil MDK，Arm Compiler 6.24）。
> 执行前请先读仓库根目录的 `AGENTS.md`，并按其中「每次修改前重新读取当前文件」的要求操作。

---

## 0. 先确认事实（不要凭这篇文档猜，改前逐条核对代码）

| 项 | 当前值 | 出处 |
| --- | --- | --- |
| 系统时钟 | HSI 16 MHz，`PLL.PLLState = RCC_PLL_NONE`，AHB/APB1/APB2 全不分频（SYSCLK = HCLK = PCLK2 = 16 MHz） | `Core/Src/main.c` 的 `SystemClock_Config` |
| 载波 | TIM8 中心对齐、`Prescaler=0`、`Period=800` → 16 MHz / (2×800) = **10 kHz** | `Core/Src/main.c` 的 `MX_TIM8_Init` |
| PWM 输出脚 | PC6/PC7/PC8 = TIM8_CH1/2/3，PC10/PC11/PC12 = TIM8_CH1N/2N/3N，AF4 | `Core/Src/stm32g4xx_hal_msp.c` |
| 调制更新 | TIM5 中断里调 `Bipolar_pwm()`；`INV_SAMP_NUMBER=360` 点/周，`ARR=888`（50 Hz 时约 17.99775 kHz） | `Core/Inc/Bipolar_PWM.h`、`Core/Src/Bipolar_PWM.c` |
| ADC1 | 3 通道扫描（IN6/IN7/IN8 = PC0/PC1/PC2），`ContinuousConvMode=DISABLE`，DMA 循环 6 半字（两帧），外部触发上升沿 | `Core/Src/main.c` 的 `MX_ADC1_Init`、`Core/Src/inverter_sampling.c` |
| ADC 触发源 | 应当是 TIM8 的 `TRGO`（`sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE`） | `Core/Src/main.c` 的 `MX_TIM8_Init` |

如果上表和那份代码对不上（例如时钟不是 16 MHz、`INV_SAMP_NUMBER` 不是 360），先停下来报告差异，不要按本文档硬改。

---

## 1. 必须修的问题：ADC 触发源指向从未初始化的 TIM1

### 症状

ADC 是「外部触发 + 不连续转换」模式：没有触发边沿就一次都不转换。原代码触发源写成 `ADC_EXTERNALTRIG_T1_TRGO`，但这份工程里 TIM1 既没有 `MX_TIM1_Init`，也没有 `__HAL_RCC_TIM1_CLK_ENABLE`，更没有 `TIM1->` 的任何访问。结果：

- `HAL_ADC_Start_DMA()` 之后 `sampling_data` 恒为 0，DMA 半满/全满回调永不触发；
- `INV_SamplingNotifyCycle()` 的周期计数不涨，`offset_valid` 永远为 0；
- 电压闭环永远不动作，菜单里 RMS 显示 `--.-`。

这是「从 TIM1 迁到 TIM8」时漏改的触发源，不是新增缺陷。

### 1.1 `Core/Src/main.c`（`MX_ADC1_Init`，约第 244 行）

找到：

```c
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T1_TRGO;
```

改为：

```c
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
```

不要改相邻的 `ExternalTrigConvEdge`（应保持 `ADC_EXTERNALTRIGCONVEDGE_RISING`）、不要开 `ContinuousConvMode`、不要改 `NbrOfConversion`。

### 1.2 `g474_code.ioc`（第 9 行）

找到：

```ini
ADC1.ExternalTrigConv=ADC_EXTERNALTRIG_T1_TRGO
```

改为：

```ini
ADC1.ExternalTrigConv=ADC_EXTERNALTRIG_T8_TRGO
```

只改这一行的值。`ADC1.IPParameters` 列表里已有 `ExternalTrigConv` 这个名字，不要增删列表项。该文件是 UTF-8 无 BOM、LF 换行，保持原样写回（脚本改写时注意不要引入 BOM，也不要打乱 `Mcu.Pin*` / `ADC1.*` 的既有排序）。

### 1.3 顺带的主机检查修复：`Core/Src/inverter_menu.c`（约第 23 行）

`tests/run_checks.cmd` 走 gcc 分支时带 `-Werror`，当前代码编不过，报：

```
Core/Src/inverter_menu.c:67:60: error: ' RMS:' directive output may be truncated writing 5 bytes into a region of size between 2 and 12 [-Werror=format-truncation=]
...
'snprintf' output between 14 and 32 bytes into a destination of size 17
```

原因：`DrawPage()` 里 `char buffer[17];` 只够显示 16 字符，但格式化串按 `%ld`/`%lu` 全量程最坏可写出 40 字节以上。

找到：

```c
    char buffer[17];
```

改为（注释是必需的，说明 17→48 不是随手写的）：

```c
    /* 显示按 16 字符截断，缓冲区留足 %ld/%lu 全量程，避免 snprintf 截断告警。 */
    char buffer[48];
```

屏幕截断由 `OLED_ShowString` 负责（8×16 字模，16 字符/行），不要改绘制逻辑。

### 1.4 文档纠正：`AGENTS.md` 第 5 行

原句（三个错误：载波频率、把 TIM1 遗留物当现状、声称有刹车保护）：

```
本项目为 STM32G474VETx 三相 SPWM 逆变器固件，使用 TIM1 生成 50 kHz 互补 PWM，支持硬件死区和刹车保护，通过 TIM5 更新调制，配有 ADC1 DMA 采样、矩阵键盘和 SSD1306 OLED。
```

改为：

```
本项目为 STM32G474VETx 三相 SPWM 逆变器固件，使用 TIM8 生成 10 kHz 互补 PWM，带硬件死区，输出由 MOE 与 PWM_EN 软件关断保护，通过 TIM5 更新调制，配有 ADC1 DMA 采样、矩阵键盘和 SSD1306 OLED。
```

依据：TIM8 是唯一的载波源；`BDTR.DeadTime=5`、`CKD=DIV1` → 硬件死区 312.5 ns；`BreakState=TIM_BREAK_DISABLE`，工程里没有 BKIN 输入，所以「刹车保护」只在软件侧（`INV_OutputDisable()` 清 `MOE`，同时拉 `INV_OFFLINE` / `PWM_EN`）。若那份代码已经有 TIM8 迁移的提交，这里的「TIM1 → TIM8」可能已经是 TIM8，只改「50 kHz → 10 kHz」和「刹车保护」那半句。

---

## 2. 如果那份副本基线更老（还留着 TIM1 的引脚配置）

症状：`g474_code.ioc` 里还能搜到 `TIM1`，`Core/Src/main.c` 的 `MX_GPIO_Init` 里有三组 `GPIO_AF6_TIM1` 初始化（PA6 上拉 + PA7/PA8/PA9/PA10、PB0/PB1 推挽）。

这是把 PWM 从 TIM1 迁到 TIM8 时留下的死配置，必须先清掉，再按第 1 节改 ADC：

- 删除的引脚键：`PA6`（TIM1_BKIN）、`PA7`（TIM1_CH1N）、`PA8`（TIM1_CH1）、`PA9`（TIM1_CH2）、`PA10`（TIM1_CH3）、`PB0`（TIM1_CH2N）、`PB1`（TIM1_CH3N），以及配套的 `SH.S_TIM1_CH1`、`SH.S_TIM1_CH2`、`SH.S_TIM1_CH3` 段；
- 同时删除 `main.c` 的 `MX_GPIO_Init` 里那三组对应初始化（`Alternate = GPIO_AF6_TIM1`）；
- `Mcu.Pin*` 重新编号：**值按物理引脚顺序保持连续（0..N），键按字符串字典序（Pin0, Pin1, Pin10..Pin19, Pin2, Pin20…）**，`Mcu.PinsNb` 由 44 改为 37；
- 保留下来的 `TIM1` 字样只应出现在 `RCC.HRTIM1Freq_Value` / `RCC.LPTIM1Freq_Value` 这类无关的时钟预算项里。

如果那份副本已是 TIM8 分支（`g474_code.ioc` 搜不到 `TIM1_BKIN`），跳过本节。

---

## 3. 明确不要做的事

1. **不要改 `INV_SAMP_NUMBER=360`、不要改 TIM5 的 `Period=888`、不要改 `INV_SetFrequency()` 的 ARR 计算。** 「调制更新率与 10 kHz 载波对齐」的改动已被用户明确暂停（见第 5 节第 1 条），改它需要连带重写主机检查里的 SPWM 相位校验。
2. **不要用 CubeMX 重新生成** `main.c` / `g474_code.ioc` 覆盖这些修改；也不要改 `.ioc` 里除第 1.2 节那一行之外的内容。
3. **不要动 `Drivers/`**（厂商 HAL/CMSIS）。
4. **不要「顺手」改 ADC**：不要开连续转换、不要改 DMA 缓冲长度（6 半字 = 两帧）、不要改 `Overrun`、不要改采样时间。
5. **不要提交 `MDK-ARM/g474_code/` 下的 `.o/.axf/.hex/.map/.htm/.dep`。** 这些文件在 git 里被跟踪，每次构建都会变脏；提交时按仓库规范排除。
6. 不要为这些改动新增测试文件：本次是配置/常量级修改，跑现有主机检查即可。

---

## 4. 验收标准

在仓库根目录执行（`UV4.exe` 需在 PATH，否则按 `AGENTS.md` 用完整路径）：

```powershell
UV4.exe -b MDK-ARM\g474_code.uvprojx -t g474_code -o build.log
cmd /c tests\run_checks.cmd
```

期望：

- 构建 `0 Error(s), 0 Warning(s)`，`build.log` 无 warning；
- 主机检查输出：

  ```
  inverter_check: PASS (SPWM, frequency, amplitude, output, RMS, PI, ADC sampling/offset, menu, OLED)
  keyboard_scan_check: PASS
  ```

- 改动范围自查：`Core/Src/main.c` 除 ADC 触发那一行外无其它变化；`grep -rn "T1_TRGO" Core g474_code.ioc` 无命中；`grep -n "ExternalTrigConv" Core/Src/main.c` 只有 `T8_TRGO` 一条。

---

## 5. 已知但本次不改的问题（供后续决策，不要自作主张动手）

1. **调制更新率与载波不对齐。** TIM5 速率是 `INV_SAMP_NUMBER × f`（360×f），只有 50 Hz 时等于 10 kHz；30 Hz 是 10.8 kHz、100 Hz 是 36 kHz。载波每次 UEV 只取一次最新 CCR，多出来的写入被覆盖，所以有效占空比刷新率恒为载波频率，频点非 50 Hz 时存在采样相位抖动。对齐方案：TIM5 固定 10 kHz（`ARR=1599`）+ Q16 相位累加器按 f 推进（50 Hz 步进 1.8 个表点，30 Hz 0.72），代价是同步重写 `tests/inverter_check.c` 的 SPWM 段（现在用 360 点整数索引校验三相 120°/240° 关系）。**用户已暂停，不要动。**
2. **没有硬件刹车输入。** `BreakState`/`Break2State` 都是 DISABLE，没配 BKIN 引脚。补上需要板级故障信号（过流比较器等）与引脚信息；悬空 BKIN 会被噪声误触发而静默关断输出，必须先确认接线。
3. **ADC 触发间隔未实测。** 名义 20 kHz（中心对齐上下顶点各一次 UEV），每次 3 通道约 11.25 µs（ADC 时钟 16 MHz，每路 47.5+12.5 周期）。但 `Bipolar_pwm()` 写 CCR 期间会置 `TIM8->CR1.UDIS`，屏蔽期间那次 UEV 不产生，实际采样率会略低于名义值，需上板确认。
4. **构建产物在 git 里。** `MDK-ARM/g474_code/` 下 8 个文件被跟踪，每次构建都产生脏改动，与仓库规范冲突，尚未处理。

---

## 6. 硬件验证（本次未做，主机模拟不能替代）

- ADC 侧：在 `HAL_ADC_ConvCpltCallback` 里翻转一个空闲 GPIO，或用调试器读 `sampling_data.sequence` 的增速，确认约 20 kHz（改前应当是 0）；
- PWM 侧：测 PC6/PC7/PC8 对 PC10/PC11/PC12，确认 10 kHz、互补、死区约 312 ns，且 `MOE` 受 `INV_OutputEnable()` / `INV_OutputDisable()` 控制；
- 若能看到相电流，确认正负半周对称、无直流偏置（偏置由 `INV_SamplingUpdateOffset()` 逐周期估计）。