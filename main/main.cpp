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
#include "io/encoder.h"
#include <esp_timer.h>
#include "nvs_flash.h"
#include "nvs.h"

Hub75Driver hub75_driver;
Display display(&hub75_driver);
RotaryEncoders encoders;
Simulation sim;
Bluetooth bt;
uint64_t overlay_expiry;
bool preset_changed;
uint64_t save_configuration_time;

const int spawn_chances[25] =
    {
        0,
        1,
        10,
        20,
        25,
        50,
        75,
        100,
        150,
        200,
        250,
        300,
        500,
        750,
        1000,
        1250,
        1500,
        1750,
        2000,
        2500,
        3000,
        3500,
        4000,
        4500,
        5000,
};

const char *APP_TAG = "APP";

#define TICK_SIZE (10)

#define OVERLAY_TIME (1500LL * 1000LL)
#define CONFIGURATION_SAVE_DELAY (10LL * 1000LL * 1000LL)

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
        4096,
        NULL,
        1,
        NULL,
        1);
}

void check_encoders(uint64_t now)
{
    int feed_delta = encoders.get_and_reset_feed_rate();

    if (feed_delta != 0)
    {
        float fr = sim.feed_rate();

        fr += feed_delta / 10000.0f;

        sim.set_feed_rate(fr);

        ESP_LOGI(APP_TAG, "Feed Rate %d [%0.4f]", feed_delta, fr);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    int kill_delta = encoders.get_and_reset_kill_rate();

    if (kill_delta != 0)
    {
        float kr = sim.kill_rate();

        kr += kill_delta / 10000.0f;

        sim.set_kill_rate(kr);

        ESP_LOGI(APP_TAG, "Kill Rate %d [%0.4f]", feed_delta, kr);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    click_type_t feed_click = encoders.is_feed_rate_clicked(now);

    if (feed_click == BUTTON_CLICKED)
    {
        overlay_expiry = now + OVERLAY_TIME;

        int chance = sim.spawn_chance();

        printf("Change = %d ", chance);

        for (int i = 0; i < 25; i++)
        {
            if (chance == spawn_chances[i])
            {
                int next_chance = spawn_chances[(i + 1) % 25];

                printf("Match: setting to %d\n", next_chance);
                chance = sim.set_spawn_chance(spawn_chances[(i + 1) % 25]);
                break;
            }
            else if (chance < spawn_chances[i])
            {
                int next_chance = spawn_chances[i];
                printf("Nearby: setting to %d\n", next_chance);
                chance = sim.set_spawn_chance(spawn_chances[i]);
                break;
            }
            else
            {
                printf("%d [%d > %d] ", i, chance, spawn_chances[i]);
            }
        }

        ESP_LOGI(APP_TAG, "Select next chance: %d", chance);

        preset_changed = false;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }
    else if (feed_click == BUTTON_HELD)
    {
        sim.add(random() % 64, random() % 32);
    }

    click_type_t kill_click = encoders.is_kill_rate_clicked(now);

    if (kill_click == BUTTON_CLICKED)
    {
        preset_changed = true;

        int preset = sim.preset() + 1;

        sim.set_preset(preset);

        ESP_LOGI(APP_TAG, "Select next preset: %d", preset);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }
    else if (kill_click == BUTTON_HELD)
    {
        sim.next_hue();
    }
}

void check_bt(uint64_t now)
{
    int16_t value;

    if (bt.feed_rate_updated(&value))
    {
        float v = value / 10000.0f;

        sim.set_feed_rate(v);

        ESP_LOGI(APP_TAG, "Feed Rate: [%d] %0.4f%%", value, v);

        overlay_expiry = now + OVERLAY_TIME;
        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    if (bt.kill_rate_updated(&value))
    {
        float v = value / 10000.0f;

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
        int16_t x = (value >> 8) & 0xFF;
        int16_t y = value & 0xFF;
        sim.add(x, y);

        ESP_LOGI(APP_TAG, "Add: %d, %d", x, y);
    }

    if (bt.hue_command())
    {
        sim.next_hue();

        ESP_LOGI(APP_TAG, "Next hue");

        save_configuration_time = now + CONFIGURATION_SAVE_DELAY;
    }

    if (bt.reset_command())
    {
        sim.reset();

        ESP_LOGI(APP_TAG, "Reset");
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

void state_supplier(sim_state_t *state)
{
    state->feed_rate = sim.feed_rate();
    state->kill_rate = sim.kill_rate();
    state->preset = sim.preset();
    state->spawn_chance = sim.spawn_chance();
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

    int p21 = gpio_get_level(GPIO_NUM_21);
    int p38 = gpio_get_level(GPIO_NUM_38);
    int p39 = gpio_get_level(GPIO_NUM_39);
    int p40 = gpio_get_level(GPIO_NUM_40);
    int p41 = gpio_get_level(GPIO_NUM_41);
    int p42 = gpio_get_level(GPIO_NUM_42);

    int p2 = (p21 << 6) + (p38 << 5) + (p39 << 4) + (p40 << 3) + (p41 << 2) + (p42);

    if (p2 != p)
    {
        printf("21=%d 38=%d 39=%d 40=%d 41=%d 42=%d\n", p21, p38, p39, p40, p41, p42);
    }

    p = p2;
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
        // debug();
        uint64_t now = esp_timer_get_time();

        check_bt(now);
        check_encoders(now);

        sim.simulate();

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                uint16_t c = sim.get_colour(x, y);

                display.draw_pixel(x, y, 0xFFF & c);
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
            ESP_LOGW(APP_TAG, "Simulator loop took too long [%dus]\n", took);
        }

        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}

extern "C" void app_main(void)
{
    nvs_flash_init();

    bt.init(state_supplier);

    hub75_driver.init();

    encoders.init();

    display.init();

    int w = display.width();
    int h = display.height();

    sim.init(w, h);

    sim_configuration_init();

    overlay_expiry = esp_timer_get_time() + 2000000LL;

    xTaskCreatePinnedToCore(sim_task, "sim_task", 4096, NULL, 5, NULL, 1);
}
