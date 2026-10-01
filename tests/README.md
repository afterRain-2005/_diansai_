# Keyboard verification

From the repository root, with GCC on PATH:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Itests/keyboard_mock -ICore/Inc Core/Src/Keyboard.c tests/keyboard_scan_check.c -o tests/keyboard_scan_check.exe
.\tests\keyboard_scan_check.exe
```

The host check covers all 16 positions, debounce, hold/release behavior,
multi-key rejection, initialization, and interrupt-state preservation.
It does not verify electrical settling, actual wiring, or the OLED.

On the STM32G474 board, connect rows R1–R4 to PE0–PE3 and columns C1–C4
to PE4–PE7. `Keyboard_Init()` overrides CubeMX's push-pull/no-pull defaults
with open-drain rows and pulled-up inputs; retain its call after GPIO init.
No EXTI or additional timer configuration is needed. SysTick must remain 1 ms.

Build target `g474_code` in Keil, flash, and check that the OLED shows
`KEYBOARD TEST`, the last key, and a press count. Expected layout:

```text
1 2 3 A
4 5 6 B
7 8 9 C
* 0 # D
```

Press each position, hold it (count increases once), then release and press
again (count increases again). Simultaneous presses are suppressed until
all keys are released for about 20 ms. Adjust `keymap` in `Keyboard.c` if
the physical keypad legends or connector order differ. The last key stays
on screen after release. A single pending event is retained; additional
presses can be dropped if the main loop is stalled.
