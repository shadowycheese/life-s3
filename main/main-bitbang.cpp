#include <stdint.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// HUB75 pin assignments
#define PIN_B1 GPIO_NUM_1
#define PIN_R1 GPIO_NUM_2
#define PIN_G1 GPIO_NUM_3
#define PIN_R2 GPIO_NUM_4
#define PIN_B2 GPIO_NUM_5
#define PIN_G2 GPIO_NUM_6
#define PIN_CLK GPIO_NUM_7
#define PIN_LAT GPIO_NUM_8
#define PIN_OE GPIO_NUM_9
#define PIN_A GPIO_NUM_10
#define PIN_B GPIO_NUM_11
#define PIN_C GPIO_NUM_12
#define PIN_D GPIO_NUM_13

static const gpio_num_t output_pins[] = {
    PIN_B1, PIN_R1, PIN_G1,
    PIN_R2, PIN_B2, PIN_G2,
    PIN_CLK, PIN_LAT, PIN_OE,
    PIN_A, PIN_B, PIN_C, PIN_D};

static void init_gpio(void)
{
    uint64_t mask = 0;

    for (int i = 0; i < sizeof(output_pins) / sizeof(output_pins[0]); i++)
        mask |= 1ULL << output_pins[i];

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&cfg);

    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_LAT, 0);
    gpio_set_level(PIN_OE, 1); // Display disabled
}

static void set_row_address(int row)
{
    gpio_set_level(PIN_A, (row >> 0) & 1);
    gpio_set_level(PIN_B, (row >> 1) & 1);
    gpio_set_level(PIN_C, (row >> 2) & 1);
    gpio_set_level(PIN_D, (row >> 3) & 1);
}

// Six vertical colour bars
static void set_pixel_bus(int x)
{
    int bar = (x * 6) / 64;

    int r = (bar == 0 || bar == 3 || bar == 5);
    int g = (bar == 1 || bar == 3 || bar == 4);
    int b = (bar == 2 || bar == 4 || bar == 5);

    // Upper half
    gpio_set_level(PIN_R1, r);
    gpio_set_level(PIN_G1, g);
    gpio_set_level(PIN_B1, b);

    // Lower half
    gpio_set_level(PIN_R2, r);
    gpio_set_level(PIN_G2, g);
    gpio_set_level(PIN_B2, b);
}

static void scan_row(int row)
{
    // Blank display while shifting data
    gpio_set_level(PIN_OE, 1);
    gpio_set_level(PIN_LAT, 0);
    gpio_set_level(PIN_CLK, 0);

    set_row_address(row);

    // Shift 64 pixels into the panel
    for (int x = 0; x < 64; x++)
    {
        set_pixel_bus(x);

        gpio_set_level(PIN_CLK, 1);
        gpio_set_level(PIN_CLK, 0);
    }

    // Latch the shifted data
    gpio_set_level(PIN_LAT, 1);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_LAT, 0);

    // Display this row pair
    gpio_set_level(PIN_OE, 0);
    esp_rom_delay_us(500);
    gpio_set_level(PIN_OE, 1);
}

extern "C" void app_main(void)
{
    init_gpio();

    while (1)
    {
        for (int row = 0; row < 16; row++)
            scan_row(row);
    }
}