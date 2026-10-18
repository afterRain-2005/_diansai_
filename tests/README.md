# 主机验证

在仓库根目录执行：

```powershell
cmd /c tests\run_checks.cmd
```

脚本使用 MSVC 或 PATH 中的 GCC，检查三相 SPWM 相位、占空比范围、频率和幅度设置、启停、两项参数菜单、OLED 裁剪与 SPI 重试，以及键盘扫描和去抖。

当前运行路径为 SPWM 调制 + ADC DMA 采样 + 电压闭环：`inverter_sampling.c` 的 DMA 回调逐帧调用 `INV_VoltageControlTick()`，实测 RMS 每帧刷新供显示，目标电压大于 0 且输出已开启时每满一个基波周期窗口执行一次 PI 写 `INV_Amp`。窗口长度按 `名义采样率/频率` 四舍五入重算，50 Hz 为 400 点、30 Hz 为 667 点、100 Hz 为 200 点，改频率时清空旧窗口重填。上电默认值由 `inverter_control_tools.h` 定义，目前为 50 Hz、90% 幅度、目标 0 V（只显示不调节），幅值范围 1%～99%；TIM1 的死区和 Break 设置沿用 CubeMX。A 键切换启停，C 键停止，D 键进入菜单；菜单内 2/8 在频率和目标电压之间切换，4/6 减加（频率 1 Hz、目标 0.5 V），经 `INV_Menu_ApplyParams()` 下发给控制层，C/D 返回。参数立即生效，重启恢复默认值。显示数据统一取自 `INV_GetParams()` 的一致性快照。

`inverter_sampling.c` 使用 ADC1 Rank1/IN6/PC0、Rank2/IN7/PC1、Rank3/IN8/PC2，依次为电压1、电压2、电流。规则序列已同步为三个 Rank；六个半字的循环 DMA 缓冲区分为两帧，半满和全满回调各处理一帧。启动顺序为外设初始化、`INV_SamplingStart()` 校准并就绪 DMA、`INV_Init()` 启动 TIM1/TIM5。

`inverter_sampling.h` 中三组 K/B 是 `value = K * ADC原始计数 + B` 的线性拟合参数，当前为一组实测拟合值，尚需按实际分压/采样电路复核。`INV_SamplingGetData(&data)` 读取一致快照，包含原始值 `raw`、拟合值 `value`、上一周均值 `offset`、扣偏置值 `ac`、`offset_valid` 和已处理帧号 `sequence`。快照用于查看最新数据，不能通过低速主循环读取快照代替逐点处理来计算 RMS。

正弦表索引进入 0 时调用 `INV_SamplingNotifyCycle()`。DMA 入口通过 `INV_SamplingUpdateOffset()` 观察周期号变化，以上一完整周期各路原始均值建立偏置；启动后的部分周期丢弃，再收满一周后才置 `offset_valid=1`。本周固定使用上一周偏置，公式为 `offset=K*mean(raw_previous_cycle)+B`，`ac=value-offset`。周期按 DMA 回调观察到的边界划分，存在采样帧量级的边界量化/延迟；不假定每周固定 200 或 400 点。该方法会同时去掉信号本身的直流分量，适用于本次需要去均值的交流反馈。

当前 SYSCLK/HCLK 为 HSI 16 MHz，APB1/APB2 均不分频，TIM1/TIM5 时钟均为 16 MHz。TIM1 中心对齐、PSC=0、ARR=800，载波仍为 10 kHz；三相初始 CCR 均为 400。RCR=0、TRGO=Update，上下溢出名义上产生 20 kHz ADC 触发；UDIS 临时屏蔽和软件 UG 会影响触发间隔。ADC 同步不分频，时钟为 16 MHz，每路采样 47.5 周期加 12.5 周期转换，三路扫描约 11.25 us。TIM5 默认 ARR=888，更新约 17.99775 kHz，对应基波约 49.99375 Hz；菜单更改频率时会重算 ARR。死区 DTG=5、CKD=DIV1，对应 312.5 ns；SPI2 保留 /32，SCK 为 500 kHz。HSE_VALUE=8 MHz 保留为外部晶振参数，当前未将其选作系统时钟。

实际处理耗时、采样间隔、丢样、HSI 时钟误差及标定精度需上板测量，不能将名义触发频率当作已验证采样率。窗口长度按名义 20 kHz 折算成整数个基波周期（`20000/频率` 四舍五入，范围 200～667 点），接入前仍须按实测采样率确认。

`inverter_control_tools.c` 提供以下接口，采样回调与菜单都通过它们接入闭环：

- `INV_ResetVoltageControl()`：清除滑动窗和积分状态；开始闭环或改变采样配置时调用，关闭输出也会自动调用。
- `INV_CalculateVoltageRms(voltage)`：每个等间隔新样本调用一次，输入为实际瞬时电压（V），须先去掉采样电路的零点偏置。使用最近一个窗口（`20000/频率` 点）内的样本计算 `sqrt(sum(v*v)/N)`，窗口未满返回 `-1.0f`。
- `INV_VoltagePI(target_rms, measured_rms, kp, ki, dt)`：误差为目标减实测；`kp` 单位 %/V，`ki` 单位 %/(V*s)，`dt` 是实际 PI 调用间隔（s）。增量式，`Δu = kp*(e-e_prev) + ki*e*dt` 叠在上一次幅值上，没有积分累加器，饱和后不会积攒退出量；首次调用取 `e_prev=0`。输出四舍五入为整数百分比，再调用 `INV_SetAmplitude()`。未启用输出、RMS 或增益为负、dt 不大于 0 时不调节幅值（也不更新上一次误差）。
- `INV_VoltageControlTick(voltage)`：采样回调逐帧调用的入口。始终累加平方和并刷新 `INV_GetVoltageRms()` 供显示；目标电压大于 0 且输出已开启时转给 `INV_UpdateVoltageControl()`，否则只测量不调幅。
- `INV_GetParams(&params)`：返回频率、目标电压、当前幅值、实测 RMS 和启停状态的一致性快照，菜单显示用它取数。
- `INV_Menu_ApplyParams(frequency, target_voltage)`：菜单下发参数的唯一入口，菜单只负责编辑，钳位和写 TIM5 由控制层完成。

RMS/PI 不检查 NaN、无穷大或计算溢出；调用方须保证输入及中间运算结果为有限数值。

`INV_UpdateVoltageControl(voltage, target_rms, kp, ki, dt)` 将 RMS 和 PI 串接：每个新样本调用一次，逐帧只累加平方和（O(1)），每满一个窗口（当前频率的一周期）才开方并执行一次 PI，因此 `dt` 传两次 PI 的间隔（s），即一个窗口时长（1/频率）。当前增益为 `2.0f` %/V 与 `20.0f` %/(V*s)，`INV_VoltageControlTick()` 按当前频率传 `dt`。

这些函数共用单组状态，须在同一执行上下文串行调用。先复位，再逐点送入电压。窗口长度随基波频率重算，始终保持整数个周期；改频率会清空旧窗口，等新窗口填满后 RMS 与 PI 才恢复。主机检查覆盖窗口填充、滑动替换、正弦 RMS、随频率重算窗口、PI 方向、饱和行为、分频调幅、闭环关闭时只测不调，以及停机复位；尚未验证采样时序和实际闭环稳定性。

这些检查不能替代实际电气验证、驱动器极性检查或功率级测试。

On the STM32G474 board, connect rows R1–R4 to PE0–PE3 and columns C1–C4
to PE4–PE7. `Keyboard_Init()` overrides CubeMX's push-pull/no-pull defaults
with open-drain rows and pulled-up inputs; retain its call after GPIO init.
No EXTI or additional timer configuration is needed. SysTick must remain 1 ms.

当前库保留项目连接器方向，逻辑映射如下：

```text
D C B A
# 9 6 3
0 8 5 2
* 7 4 1
```

键盘只保留一个待处理事件，主循环长时间阻塞时后续按键可能丢失；C 键优先占用待处理事件，由主循环处理停机。
