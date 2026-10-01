# Repository Guidelines

## Project Structure & Module Organization

This is STM32G474VETx firmware generated with STM32CubeMX and built with Keil MDK. The current application initializes SPI2 and displays a test screen on a 128×64 SSD1306 OLED.

- `Core/Src/`: application entry point, interrupt handlers, HAL initialization, and `oled.c`; font data is embedded in the OLED driver.
- `Core/Inc/`: matching public headers and HAL configuration.
- `Drivers/`: bundled STM32G4 HAL and CMSIS dependencies. Avoid unrelated vendor edits.
- `MDK-ARM/`: Keil project, startup assembly, linker scatter file, and build outputs.
- `g474_code.ioc`: CubeMX peripheral, pin, and clock configuration.

## Build, Test, and Development Commands

Open `MDK-ARM/g474_code.uvprojx` in Keil µVision, select target `g474_code`, and build with F7. The recorded build used Arm Compiler 6.24.

If `UV4.exe` is available on PATH, run from the repository root:

```powershell
UV4.exe -b MDK-ARM\g474_code.uvprojx -t g474_code -o build.log
```

Inspect `build.log` for errors and warnings. Run firmware by downloading it to the board through µVision's configured debugger. No application Makefile, CMake build, or automated test command is provided.

## Coding Style & Naming Conventions

Follow surrounding C style: two-space indentation in generated code, four spaces in the OLED module, and existing brace placement. Use matching `.c`/`.h` module names, `OLED_*` public display functions, and uppercase macros such as `OLED_WIDTH`. Keep internal helpers and buffers `static`. No formatter or linter configuration is present.

Put custom changes in CubeMX `USER CODE` blocks where available. Update `.ioc` for peripheral changes and inspect regeneration diffs. Add new source files to the Keil project explicitly.

## Testing Guidelines

No application test framework or coverage threshold is configured; CMSIS test suites are bundled vendor material. Build firmware changes and verify affected behavior on hardware. For display changes, check startup text, drawing boundaries, and refresh behavior. Record board, wiring, and results. For non-trivial hardware-independent logic, add a focused runnable check such as `tests/oled_bounds_check.c` and document its command.

## Commit & Pull Request Guidelines

History contains only `init` and `时钟修复`; no strict message convention is established. Use short, descriptive subjects identifying the changed behavior. Keep commits focused and exclude incidental build outputs and personal IDE settings.

Pull requests should explain the change, list affected peripherals or pins, and report build and hardware verification. Link relevant issues and attach display photos when visual behavior changes. State explicitly when hardware testing was unavailable.
