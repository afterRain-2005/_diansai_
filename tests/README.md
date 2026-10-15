# 主机验证

在仓库根目录执行：

```powershell
cmd /c tests\run_checks.cmd
```

脚本会运行 MSVC 主机检查，覆盖死区编码、三相 PWM 相位和边界、故障关断与恢复、ADC RMS、PI 数值边界、Flash 参数记录和写入失败模拟、OLED 裁剪与 SPI 重试，以及键盘去抖和 C 键停机优先级。

这些检查不能替代实际电气验证、ADC 标定、驱动器极性检查或功率级测试。

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

键盘只保留一个待处理事件，主循环长时间阻塞时后续按键可能丢失；C 键另设独立停机锁存。
