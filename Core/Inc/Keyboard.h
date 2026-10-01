#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEYBOARD_NO_KEY '\0'

/* PE0..PE3: rows; PE4..PE7: columns.
 * Row order: 123A / 456B / 789C / *0#D. */
void Keyboard_Init(void);
/* Call once per 1 ms interrupt. No delays or display operations. */
void Keyboard_ScanISR(void);
/* Main-loop consumer: one debounced press, or KEYBOARD_NO_KEY.
 * Holding a key does not repeat; all keys must be released to rearm. */
char Keyboard_GetKey(void);

#ifdef __cplusplus
}
#endif

#endif
