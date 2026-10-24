#include "driver/gpio.h"
#include "esp/espio.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "matrix/hub75.h"
#include "matrix/hub75_driver.h"
#include "events/edt.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include "simulation/sim-1.h"
#include "simulation/sim-2.h"
#include "simulation/sim-3.h"
#include "simulation/sim-4.h"
#include <esp_timer.h>

Hub75Driver hub75_driver;
Hub75 hub75(&hub75_driver);
Simulation4 sim;

#define TICK_SIZE (10)
#define RESET_TICKS (25)
#define TICK_INC (0.01)

extern "C" void app_main(void)
{
    nvs_flash_init();

    edt_init();

    hub75_driver.init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE * 5);

    TickType_t last_wake_time = xTaskGetTickCount();

    int w = hub75.width();
    int h = hub75.height();

    sim.init(w, h);

    ESP_LOGI("APP_MAIN", "Running main loop on core %d", xPortGetCoreID());

    double time = 0.0f;

    for (;; time += TICK_INC)
    {
        uint64_t now = esp_timer_get_time();
        sim.simulate(time);
        int took = (int)(esp_timer_get_time() - now);

        // printf("%d\n", took / 1000);

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                uint16_t c = sim.get_colour(x, y);

                hub75.draw_pixel(x, y, c);
            }
        }

        hub75.commit_frame();

        vTaskDelayUntil(&last_wake_time, delay_period);
        // vTaskDelay(1);
    }
}
