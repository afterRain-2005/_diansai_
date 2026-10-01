#include <assert.h>
#include <stdio.h>
#include "main.h"
#include "Keyboard.h"

GPIO_TypeDef keyboard_test_gpio;
static uint32_t primask;
static uint8_t configured;

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, uint32_t state)
{
    if (state == GPIO_PIN_SET) port->ODR |= pins;
    else port->ODR &= (uint16_t)~pins;
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    assert(port == GPIOE);
    if (config->Pin == 0x0FU) {
        assert(config->Mode == GPIO_MODE_OUTPUT_OD);
        configured |= 1U;
    } else {
        assert(config->Pin == 0xF0U);
        assert(config->Mode == GPIO_MODE_INPUT && config->Pull == GPIO_PULLUP);
        configured |= 2U;
    }
}

uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1U; }
void __set_PRIMASK(uint32_t value) { primask = value; }

/* Model row/column contacts independently from the driver's debounce logic. */
static void sample(uint16_t contacts, unsigned ticks)
{
    while (ticks-- != 0U) {
        uint16_t active = (uint16_t)(~GPIOE->ODR & 0x0FU);
        unsigned row = 0U;
        assert(active != 0U && (active & (active - 1U)) == 0U);
        while ((active & (1U << row)) == 0U) row++;
        GPIOE->IDR = 0xF0U & ~(((contacts >> (row * 4U)) & 0x0FU) << 4U);
        Keyboard_ScanISR();
    }
}

int main(void)
{
    const char expected[] = "123A456B789C*0#D";
    Keyboard_ScanISR(); /* Safe before initialization. */
    Keyboard_Init();
    assert(configured == 3U);
    sample(0U, 24U);
    assert(Keyboard_GetKey() == KEYBOARD_NO_KEY);

    for (unsigned key = 0U; key < 16U; key++) {
        uint16_t contact = (uint16_t)(1U << key);
        sample(contact, 8U);
        sample(0U, 4U);
        sample(contact, 16U);
        assert(Keyboard_GetKey() == KEYBOARD_NO_KEY); /* Bounce/short press. */
        sample(contact, 4U);
        assert(Keyboard_GetKey() == expected[key]);
        sample(contact, 100U);
        assert(Keyboard_GetKey() == KEYBOARD_NO_KEY); /* No hold repeat. */
        sample(0U, 8U);
        sample(contact, 24U);
        assert(Keyboard_GetKey() == KEYBOARD_NO_KEY); /* Release bounce. */
        sample(0U, 20U);
    }

    sample(0x0003U, 24U); /* Two keys: reject and require full release. */
    assert(Keyboard_GetKey() == KEYBOARD_NO_KEY);
    sample(0x0001U, 24U);
    assert(Keyboard_GetKey() == KEYBOARD_NO_KEY);
    sample(0U, 20U);
    sample(0x8000U, 20U);
    primask = 1U;
    assert(Keyboard_GetKey() == 'D');
    assert(primask == 1U); /* Preserve caller's interrupt state. */
    primask = 0U;
    assert(Keyboard_GetKey() == KEYBOARD_NO_KEY && primask == 0U);

    /* A pending event remains intact if the main-loop consumer is delayed. */
    sample(0U, 20U);
    sample(1U, 20U);
    sample(0U, 20U);
    sample(2U, 20U);
    assert(Keyboard_GetKey() == '1');
    assert(Keyboard_GetKey() == KEYBOARD_NO_KEY);
    puts("keyboard_scan_check: PASS");
    return 0;
}
