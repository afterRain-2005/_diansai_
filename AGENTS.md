# 仓库规范

## 项目结构与模块组织

本项目为 STM32G474VETx 三相 SPWM 逆变器固件，使用 TIM8 生成 50 kHz 互补 PWM，支持硬件死区和刹车保护，通过 TIM5 更新调制，配有 ADC1 DMA 采样、矩阵键盘和 SSD1306 OLED。

- `Core/Src/`：应用代码，包括 `Bipolar_PWM.c`、`inverter_control_tools.c`、`inverter_menu.c`、`Keyboard.c`、`oled.c`，以及 HAL 初始化和中断处理代码。
- `Core/Inc/`：对应的头文件与配置；OLED 字模内嵌于 `oled.c`。
- `Drivers/`：厂商 HAL/CMSIS 代码，避免无关修改。
- `MDK-ARM/`：Keil 工程、启动代码、链接配置和构建产物。
- `tests/`：主机检查及硬件模拟代码，详见 `tests/README.md`。
- `g474_code.ioc`：CubeMX 配置。

## 构建、测试与开发命令

在 Keil µVision 中打开 `MDK-ARM/g474_code.uvprojx`，选择目标 `g474_code`，按 F7 构建。工程指定 Arm Compiler 6.24。若 `UV4.exe` 已加入 PATH，可在仓库根目录执行：

```powershell
UV4.exe -b MDK-ARM\g474_code.uvprojx -t g474_code -o build.log
cmd /c tests\run_checks.cmd
```

第一条命令构建固件，完成后检查 `build.log` 中的错误与警告。第二条命令使用 MSVC 编译并运行主机检查；若无法自动找到编译器，请使用 Visual Studio 的 x64 Native Tools 命令提示符。通过 µVision 中配置好的调试探针下载和调试。

## 代码风格与命名约定

遵循周围 C 代码的风格：生成代码使用两空格缩进，自定义模块通常使用四空格。源文件与头文件保持同名配对，宏使用全大写，OLED 接口使用 `OLED_` 前缀，内部辅助函数和缓冲区声明为 `static`。项目未配置格式化工具或代码检查工具。

修改生成文件时，尽量将自定义代码放入 `USER CODE` 块。外设变更须同步更新 `.ioc` 和 `main.h` 中的引脚定义；新增源文件须显式加入 Keil 工程。

不可自行写测试与保护代码，造成代码冗余。

## 测试规范

主机检查覆盖 PWM、故障处理、RMS/PI 计算、Flash 记录、OLED 行为和键盘扫描。测试文件沿用 `*_check.c` 命名，项目未设置覆盖率门槛。非简单逻辑改动须附带一个小型、可运行的回归检查。修改代码后须构建固件。单独记录开发板、接线及硬件验证结果；主机模拟不能证明实际电气行为。

## 提交与 Pull Request 规范

历史提交使用简短的中英文标题，尚无统一前缀规则。提交信息应简洁说明行为变化。保持改动聚焦，排除生成的二进制文件和个人 IDE 配置。PR 应说明涉及的外设、引脚及验证结果，适用时关联 issue；显示改动应附实物照片。未进行硬件测试时须明确说明。

## 代理工作要求

用户会手动修改代码，因此每次审查或修改前，必须重新读取当前文件并检查工作区状态。保留无关改动，以当前代码为依据，不可凭记忆判断。
