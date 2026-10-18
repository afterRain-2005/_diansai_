# 主机验证

在仓库根目录执行：

```powershell
cmd /c tests\run_checks.cmd
```

脚本使用 MSVC 或 PATH 中的 GCC，检查三相 SPWM 相位、占空比范围、频率和幅度设置、启停、两项参数菜单、OLED 裁剪与 SPI 重试，以及键盘扫描和去抖。

当前运行路径仍为开环 SPWM，不启动 ADC DMA，也不自动调用 RMS/PI。上电默认值由 `inverter_control_tools.h` 定义，目前为 50 Hz、90% 幅度，幅值范围 1%～99%；TIM1 的死区和 Break 设置沿用 CubeMX。A 键切换启停，C 键停止，D 键进入菜单；菜单内 2/8 选择频率或幅度，4/6 减加，C/D 返回。参数立即生效，重启恢复默认值。

`inverter_control_tools.c` 提供以下接口，留待独立采样文件调用：

- `INV_ResetVoltageControl()`：清除滑动窗和积分状态；开始闭环或改变采样配置时调用，关闭输出也会自动调用。
- `INV_CalculateVoltageRms(voltage)`：每个等间隔新样本调用一次，输入为实际瞬时电压（V），须先去掉采样电路的零点偏置。使用最近 `INV_RMS_WINDOW_SIZE` 个样本计算 `sqrt(sum(v*v)/N)`，窗口未满返回 `-1.0f`。
- `INV_VoltagePI(target_rms, measured_rms, kp, ki, dt)`：误差为目标减实测；`kp` 单位 %/V，`ki` 单位 %/(V*s)，`dt` 是实际 PI 调用间隔（s）。积分从当前幅值起步，带饱和抗积分累积，输出四舍五入为整数百分比，再调用 `INV_SetAmplitude()`。未启用输出、RMS 或增益为负、dt 不大于 0 时不调节幅值。

RMS/PI 不检查 NaN、无穷大或计算溢出；调用方须保证输入及中间运算结果为有限数值。

`INV_UpdateVoltageControl(voltage, target_rms, kp, ki, dt)` 将 RMS 和 PI 串接：每个新样本调用一次，只有 RMS 不等于 `-1.0f` 才调用 PI，返回本次 RMS。开始前先复位；使用此入口时不要重复单独调用 RMS/PI。满窗后每个新样本执行一次 PI，因此 `dt` 传采样间隔（s），首次积分也只使用这个间隔。

这些函数共用单组状态，须在同一执行上下文串行调用。先复位，再逐点送入电压；收到有效 RMS 后，按选定控制周期调用 PI。200 点窗口仅在 10 kHz 采样、50 Hz 基波时恰好覆盖一周期；采样频率和基波变化时须重新选择窗口，避免非整周期引入 RMS 波动。PI 增益和调用周期由后续采样文件传入，没有预设板上参数。主机检查覆盖窗口填充、滑动替换、正弦 RMS、PI 方向、积分、饱和恢复及停机复位；尚未验证采样时序和实际闭环稳定性。

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
