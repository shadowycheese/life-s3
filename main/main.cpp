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
#include "io/bluetooth.h"
#include <esp_timer.h>
#include "nvs_flash.h"
#include "nvs.h"

Hub75Driver hub75_driver;
Display display(&hub75_driver);
Simulation sim;
RotaryEncoders encoders;
Bluetooth bt;
uint64_t overlay_expiry;
bool preset_changed;
uint64_t save_configuration_time;

const char *APP_TAG = "APP";

#define TICK_SIZE (10)

#define OVERLAY_TIME (3LL * 1000LL * 1000LL)
#define CONFIGURATION_SAVE_DELAY (20LL * 1000LL * 1000LL)

esp_err_t save_configuration(const sim_configuration_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("settings", NVS_READWRITE, &handle);

    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_blob(handle, "config", config, sizeof(sim_configuration_t));

    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t load_configuration(sim_configuration_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("settings", NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    size_t size = sizeof(sim_configuration_t);
    err = nvs_get_blob(handle, "config", config, &size);

    nvs_close(handle);
    return err;
}

void sim_configuration_writer(void *)
{
    for (;;)
    {
        uint64_t time = esp_timer_get_time();

        if (save_configuration_time > 0 && save_configuration_time < time)
        {
            save_configuration_time = 0;

            sim_configuration_t config;

            sim.get_configuration(&config);

            esp_err_t err = save_configuration(&config);

            if (err == ESP_OK)
            {
                ESP_LOGI(APP_TAG, "Saved config: Feed rate: %0.5f, Kill rate: %0.5f, Seed chance: %d", config.feed_rate, config.kill_rate, config.seed_chance);
            }
            else
            {
                ESP_LOGW(APP_TAG, "Failed to save config: %d", err);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void sim_configuration_init()
{
    sim_configuration_t config;

    esp_err_t err = load_configuration(&config);

    if (err == ESP_OK)
    {
        if (config.magic != SIM_CONFIG_MAGIC)
        {
            ESP_LOGE(APP_TAG, "Loaded config, but wrong magic: %08X [expected %08X]", config.magic, SIM_CONFIG_MAGIC);
        }
        else
        {
            ESP_LOGI(APP_TAG, "Loaded config: Feed rate: %0.5f, Kill rate: %0.5f, Seed chance: %d", config.feed_rate, config.kill_rate, config.seed_chance);

            sim.set_configuration(&config);
        }
    }
    else
    {
        ESP_LOGW(APP_TAG, "Failed to load config: %d", err);
    }

    save_configuration_time = 0;

    xTaskCreatePinnedToCore(
        sim_configuration_writer,
        "sim_config_writer",
        2048,
        NULL,
        5,
        NULL,
        1);
}

void check_encoders(uint64_t now)
{
    int feed_delta = encoders.get_and_reset_feed_rate();

    if (feed_delta != 0)
    {
        float fr = sim.increment_feed_rate(((float)feed_delta) / 10000.0f);

        ESP_LOGI(APP_TAG, "Feed Rate %d [%0.4f]", feed_delta, fr);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    int kill_delta = encoders.get_and_reset_kill_rate();

    if (kill_delta != 0)
    {
        float fr = sim.increment_kill_rate(((float)kill_delta) / 10000.0f);

        ESP_LOGI(APP_TAG, "Kill Rate %d [%0.4f]", feed_delta, fr);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    click_type_t feed_click = encoders.is_feed_rate_clicked(now);

    if (feed_click == BUTTON_CLICKED)
    {
        preset_changed = true;

        int preset = sim.next_preset();

        ESP_LOGI(APP_TAG, "Select next preset: %d", preset);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
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

        ESP_LOGI(APP_TAG, "Select next chance: %d", chance);

        preset_changed = false;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }
    else if (kill_click == BUTTON_HELD)
    {
        if (overlay_expiry < now)
        {
            ESP_LOGW(APP_TAG, "Restarting!");

            esp_restart();
        }
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
    display.setTextColor(0x1FF0);
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

    // encoders.init();
    bt.init();

    hub75_driver.init();

    display.init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE * 5);

    TickType_t last_wake_time = xTaskGetTickCount();

    int w = display.width();
    int h = display.height();

    sim.init(w, h);

    sim_configuration_init();

    overlay_expiry = esp_timer_get_time() + 3000000LL;

    ESP_LOGI(APP_TAG, "Running main loop on core %d", xPortGetCoreID());

    for (;;)
    {
        uint64_t now = esp_timer_get_time();

        // check_encoders(now);

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

        if (save_configuration_time > 0)
        {
            display.draw_pixel(61, 31, 0x0C00);
            display.draw_pixel(62, 31, 0x00C0);
            display.draw_pixel(63, 31, 0x000C);
        }

        display.commit_frame();

        uint64_t t = esp_timer_get_time();

        int took = (int)(t - now);

        if (took > 50000)
        {
            ESP_LOGW(APP_TAG, "Simulator loop took too long [%dus]\n", took);
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
