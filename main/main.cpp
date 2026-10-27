#include "driver/gpio.h"
#include "io/espio.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "matrix/display.h"
#include "matrix/hub75_driver.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include "simulation/simulation.h"
#include "io/encoder.h"
#include <esp_timer.h>

Hub75Driver hub75_driver;
Display display(&hub75_driver);
Simulation sim;
RotaryEncoders encoders;
uint64_t overlay_expiry;
bool preset_changed;

#define TICK_SIZE (10)

#define OVERLAY_TIME (3LL * 1000LL * 1000LL)

void check_encoders(uint64_t now)
{
    int feed_delta = encoders.get_and_reset_feed_rate();

    if (feed_delta != 0)
    {
        float fr = sim.increment_feed_rate(((float)feed_delta) / 10000.0f);

        printf("Feed Rate %d [%0.4f]\n", feed_delta, fr);

        overlay_expiry = now + OVERLAY_TIME;
    }

    int kill_delta = encoders.get_and_reset_kill_rate();

    if (kill_delta != 0)
    {
        float fr = sim.increment_kill_rate(((float)kill_delta) / 10000.0f);

        printf("Kill Rate %d [%0.4f]\n", feed_delta, fr);

        overlay_expiry = now + OVERLAY_TIME;
    }

    click_type_t feed_click = encoders.is_feed_rate_clicked(now);

    if (feed_click == BUTTON_CLICKED)
    {
        preset_changed = true;

        int preset = sim.next_preset();

        printf("Select next preset: %d\n", preset);

        overlay_expiry = now + OVERLAY_TIME;
    }
    else if (feed_click == BUTTON_HELD)
    {
        sim.add();
    }

    click_type_t kill_click = encoders.is_kill_rate_clicked(now);

    if (kill_click == BUTTON_CLICKED)
    {
        overlay_expiry = now + OVERLAY_TIME;

        int chance = sim.chance_adjust();

        printf("Select next chance: %d\n", chance);

        preset_changed = false;
    }
    else if (kill_click == BUTTON_HELD)
    {
        printf("Restart!\n");

        esp_restart();
    }
}

void debug()
{
    static int p = 0;
    static uint64_t t;

    uint64_t t2 = esp_timer_get_time();

    if ((t2 - t) > 100000)
    {
        vTaskDelay(2);

        t = t2;
    }

    int p17 = gpio_get_level(GPIO_NUM_17);
    int p21 = gpio_get_level(GPIO_NUM_21);
    int p47 = gpio_get_level(GPIO_NUM_47);

    int p2 = (p17 << 2) + (p21 << 1) + (p47);

    if (p2 != p)
    {
        printf("17=%d 21=%d 47=%d\n", p17, p21, p47);
    }

    p = p2;
}

uint32_t dbg_pixels[128] = {0};
int dbg_pos = 0;

void debug_pixel(int id, int x, int y, int c)
{
    int val = (x << 24) | (y << 16) | c;

    dbg_pixels[id] = val;
}

void draw_overlay()
{
    display.setCursor(2, 2);
    display.setTextSize(0);
    display.setTextColor(0xFF0);
    display.printf("F:%05.2f%%", sim.feed_rate() * 100.0f);
    display.setCursor(2, 12);
    display.printf("K:%05.2f%%", sim.kill_rate() * 100.0f);
    display.setCursor(2, 22);

    if (preset_changed)
    {
        const char *preset = sim.name();

        if (strlen(preset) > 0)
        {
            display.printf("%s", preset);

            return;
        }
    }

    display.printf("C:%05.2f%%", sim.chance() / 100.0f);
}

extern "C" void app_main(void)
{
    nvs_flash_init();

    encoders.init();

    hub75_driver.init();

    display.init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE * 5);

    TickType_t last_wake_time = xTaskGetTickCount();

    int w = display.width();
    int h = display.height();

    sim.init(w, h);

    ESP_LOGI("APP_MAIN", "Running main loop on core %d", xPortGetCoreID());

    for (;;)
    {
        uint64_t now = esp_timer_get_time();

        check_encoders(now);

        sim.simulate();

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                uint16_t c = sim.get_colour(x, y);

                display.draw_pixel(x, y, c);
            }
        }

        if (now < overlay_expiry)
        {
            draw_overlay();
        }

        display.commit_frame();

        uint64_t t = esp_timer_get_time();

        int took = (int)(t - now);

        if (took > 50000)
        {
            ESP_LOGW("APP_MAIN", "Simulator loop took too long [%dus]\n", took);
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
