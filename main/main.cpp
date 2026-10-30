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
#include "io/bluetooth.h"
#include <esp_timer.h>
#include "nvs_flash.h"
#include "nvs.h"

Hub75Driver hub75_driver;
Display display(&hub75_driver);
Simulation sim;
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
                ESP_LOGI(APP_TAG, "Saved config: Feed rate: %0.5f, Kill rate: %0.5f, Spawn chance: %d", config.feed_rate, config.kill_rate, config.spawn_chance);
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
            ESP_LOGI(APP_TAG, "Loaded config: Feed rate: %0.5f, Kill rate: %0.5f, Spawn chance: %d", config.feed_rate, config.kill_rate, config.spawn_chance);

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
        1,
        NULL,
        1);
}

void check_bt(uint64_t now)
{
    int16_t value;

    if (bt.feed_rate_updated(&value))
    {
        float v = value / 100000.0f;

        sim.set_feed_rate(v);

        ESP_LOGI(APP_TAG, "Feed Rate: [%d] %0.4f%%", value, v);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    if (bt.kill_rate_updated(&value))
    {
        float v = value / 100000.0f;

        sim.set_kill_rate(v);

        ESP_LOGI(APP_TAG, "Kill Rate: [%d] %0.4f%%", value, v);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    if (bt.spawn_chance_updated(&value))
    {
        sim.set_spawn_chance(value);

        ESP_LOGI(APP_TAG, "Spawn Chance: [%d] %0.2f", value, value / 100.0f);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
        preset_changed = false;
    }

    if (bt.preset_updated(&value))
    {
        sim.set_preset(value);

        ESP_LOGI(APP_TAG, "Selected Preset: %d", value);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
        preset_changed = true;
    }

    if (bt.add_command(&value))
    {
        sim.add(value);

        ESP_LOGI(APP_TAG, "Added: %d", value);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    if (bt.reset_command())
    {
        sim.reset();

        ESP_LOGI(APP_TAG, "Reset");

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }
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

    display.printf("S:%05.2f%%", sim.spawn_chance() / 100.0f);
}

void clear_screen(int active)
{
    if (active)
    {
        display.clear_screen();
    }
}

void sim_task(void *)
{
    ESP_LOGI(APP_TAG, "Running main loop on core %d", xPortGetCoreID());

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE * 5);

    TickType_t last_wake_time = xTaskGetTickCount();

    int w = display.width();
    int h = display.height();

    for (int i = 0;; i++)
    {
        uint64_t now = esp_timer_get_time();

        check_bt(now);

        sim.simulate();

        if (false) // bt.active())
        {
            display.clear_screen();

            for (int y = 0; y < h; y++)
            {
                for (int x = y % 2; x < w; x += 2)
                {
                    uint16_t c = sim.get_colour(x, y);

                    display.draw_pixel(x, y, c);
                }
            }
        }
        else
        {
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    uint16_t c = sim.get_colour(x, y);

                    // display.draw_pixel(x, y, c);
                    // display.draw_pixel(x, y, 0xF00 & c);
                    //  display.draw_pixel(x, y, 0xF00 & ((((rand()) % 12) + 4) << 8));

                    display.draw_pixel(x, y, 0xFFF & c);
                }
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

extern "C" void app_main(void)
{
    nvs_flash_init();

    bt.init(clear_screen);

    hub75_driver.init();

    display.init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE * 5);

    int w = display.width();
    int h = display.height();

    sim.init(w, h);

    sim_configuration_init();

    overlay_expiry = esp_timer_get_time() + 3000000LL;

    xTaskCreatePinnedToCore(sim_task, "sim_task", 4096, NULL, 5, NULL, 1);
}
