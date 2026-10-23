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

Hub75Driver hub75_driver;
Hub75 hub75(&hub75_driver);

#define TICK_SIZE (10)
#define RESET_TICKS (25)

extern "C" void app_main(void)
{
    nvs_flash_init();

    edt_init();

    hub75_driver.init();

    const TickType_t delay_period = pdMS_TO_TICKS(TICK_SIZE);

    TickType_t last_wake_time = xTaskGetTickCount();

    uint16_t c = 0;

    for (int y = 0; y < 32; y++)
    {
        int c2 = (c++ % 3);

        for (int x = 0; x < 64; x++)
        {
            if (c2 == 0)
            {
                hub75.draw_pixel(x, y, 0x30);
            }
            else if (c2 == 1)
            {
                hub75.draw_pixel(x, y, 0x0C);
            }
            else if (c2 == 2)
            {
                hub75.draw_pixel(x, y, 0x03);
            }
        }
    }

    hub75.commit_frame();

    ESP_LOGI("APP_MAIN", "Running main loop on core %d", xPortGetCoreID());

    for (;;)
    {
        vTaskDelayUntil(&last_wake_time, delay_period);
    }
}
