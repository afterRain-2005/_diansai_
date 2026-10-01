#include "Keyboard.h"
#include "main.h"

#define KEYBOARD_ROWS (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)
#define KEYBOARD_COLUMNS (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7)
#define KEYBOARD_DEBOUNCE_FRAMES 5U

static const char keymap[] = "D#0*C987B654A321";
static uint8_t row;
static uint8_t stable_frames;
static uint8_t armed;
static uint16_t frame;
static uint16_t candidate;
static volatile uint8_t ready;
static volatile char pending_key;

void Keyboard_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    ready = 0U;
    __HAL_RCC_GPIOE_CLK_ENABLE();
    /* Release inactive rows; open-drain prevents opposing output levels. */
    HAL_GPIO_WritePin(GPIOE, KEYBOARD_ROWS, GPIO_PIN_SET);
    gpio.Pin = KEYBOARD_ROWS;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);
    gpio.Pin = KEYBOARD_COLUMNS;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOE, &gpio);

    row = 0U;
    stable_frames = 0U;
    armed = 1U;
    frame = 0U;
    candidate = 0U;
    pending_key = KEYBOARD_NO_KEY;
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_RESET);
    ready = 1U;
}

void Keyboard_ScanISR(void)
{
    uint16_t columns;

    if (ready == 0U) return;
    /* Selected row has settled for one tick before sampling. */
    columns = (uint16_t)((~GPIOE->IDR >> 4U) & 0x0FU);
    frame |= (uint16_t)(columns << (row * 4U));
    HAL_GPIO_WritePin(GPIOE, (uint16_t)(1U << row), GPIO_PIN_SET);
    row++;
    if (row == 4U) {
        row = 0U;
        if (frame != candidate) {
            candidate = frame;
            stable_frames = 1U;
        } else if (stable_frames < KEYBOARD_DEBOUNCE_FRAMES) {
            stable_frames++;
        }
        /* Multi-key/ghost combinations inhibit events until full release. */
        if ((frame != 0U) && ((frame & (frame - 1U)) != 0U)) {
            armed = 0U;
        }
        if (stable_frames == KEYBOARD_DEBOUNCE_FRAMES) {
            if (candidate == 0U) {
                armed = 1U;
            } else if (armed != 0U) {
                uint8_t index = 0U;
                uint16_t bit = candidate;
                while ((bit & 1U) == 0U) {
                    bit >>= 1U;
                    index++;
                }
                /* ponytail: one pending event; if main-loop stalls lose presses;
                 * migrate to a bounded event queue when every press must persist. */
                if (pending_key == KEYBOARD_NO_KEY) pending_key = keymap[index];
                armed = 0U;
            }
        }
        frame = 0U;
    }
    HAL_GPIO_WritePin(GPIOE, (uint16_t)(1U << row), GPIO_PIN_RESET);
}

char Keyboard_GetKey(void)
{
    uint32_t primask = __get_PRIMASK();
    char key;

    __disable_irq();
    key = pending_key;
    pending_key = KEYBOARD_NO_KEY;
    __set_PRIMASK(primask);
    return key;
}
