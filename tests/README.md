# 主机验证

在仓库根目录执行：

```powershell
cmd /c tests\run_checks.cmd
```

脚本使用 MSVC 或 PATH 中的 GCC，检查三相 SPWM 相位、占空比范围、频率和幅度设置、启停、两项参数菜单、OLED 裁剪与 SPI 重试，以及键盘扫描和去抖。

当前为开环 SPWM：不启动 ADC DMA，不计算 RMS，不执行 PI、软启动、软件故障检查或 Flash 参数保存。上电默认 50 Hz、10% 幅度；TIM1 的硬件死区和 BKIN 沿用 CubeMX 配置。A 键切换启停，C 键停止，D 键进入菜单；菜单内 2/8 选择频率或幅度，4/6 减加，C/D 返回。参数立即生效，重启恢复默认值。

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
