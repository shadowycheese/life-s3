#ifndef Bluetooth_H
#define Bluetooth_H

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "esp_log.h"
#include "nvs_flash.h"

// NimBLE Stack Headers
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

int gatt_svr_write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

void nimble_host_task_wrapper(void *param);
void ble_app_on_sync_wrapper(void);

class Bluetooth
{
public:
    Bluetooth()
    {
        instance = this;
    };

    void init()
    {
        _gatt_svr_svcs[0] = {};
        _gatt_svr_svcs[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
        _gatt_svr_svcs[0].uuid = &_gatt_svc_uuid.u;
        _gatt_svr_svcs[0].characteristics = _bt_characteristics;
        _gatt_svr_svcs[1] = {0}; // No more services

        nimble_port_init();
        ble_hs_cfg.sync_cb = ble_app_on_sync_wrapper;

        // 3. Initialize custom GATT tables
        ble_svc_gap_init();
        ble_svc_gatt_init();
        ble_gatts_count_cfg(_gatt_svr_svcs);
        ble_gatts_add_svcs(_gatt_svr_svcs);

        // Set default network name inside GAP
        ble_svc_gap_device_name_set("Matrix-Remote");

        // 4. Spin up the host task loop
        // (Note: Task pinning here is a safety fallback; the 'menuconfig' setting dominates affinity)
        xTaskCreatePinnedToCore(nimble_host_task_wrapper, "nimble_host", 4096, NULL, 5, NULL, 1);
    }
    static constexpr char *TAG = "BLE_CTRL";

private:
    int write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
    {
        if (ctxt->om->om_len >= 2)
        {
            uint8_t command = ctxt->om->om_data[0];
            uint8_t value = ctxt->om->om_data[1];

            if (command == 0x42)
            {                                               // 'B' for Brightness
                _sim_feed_rate = (value > 63) ? 63 : value; // Safety boundary clamp
                ESP_LOGI(TAG, "New Brightness parsed on Core 1: %d", _sim_feed_rate);
            }
            else if (command == 0x4D)
            {                                 // 'M' for Mode Action
                _sim_feed_rate = (char)value; // Will hold 'N' or 'P'
                ESP_LOGI(TAG, "New Mode command parsed on Core 1: %c", _sim_feed_rate);
            }
        }
        return 0;
    }

    void ble_app_advertise(void)
    {
        struct ble_gap_adv_params adv_params;
        struct ble_hs_adv_fields fields;
        memset(&fields, 0, sizeof(fields));

        fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
        fields.name = (uint8_t *)"Cells-Remote";
        fields.name_len = strlen("Cells-Remote");
        fields.name_is_complete = 1;

        ble_gap_adv_set_fields(&fields);

        memset(&adv_params, 0, sizeof(adv_params));
        adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
        ble_gap_adv_start(_ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    }

    void ble_app_on_sync(void)
    {
        ESP_LOGI(TAG, "ble_hs_id_infer_auto");
        ble_hs_id_infer_auto(0, &_ble_addr_type);
        ESP_LOGI(TAG, "ble_app_advertise");
        ble_app_advertise();
        ESP_LOGI(TAG, "ble_app_on_sync complete");
    }

    // Dedicated FreeRTOS background task running host execution state
    void nimble_host_task()
    {
        nimble_port_run();
        nimble_port_freertos_deinit();
    }

    uint8_t _ble_addr_type;

    // Your atomic cross-core global matrix variables
    volatile uint32_t _sim_feed_rate = 32;
    volatile uint32_t _sim_kill_rate = 33;
    volatile uint32_t _sim_preset = 34;
    volatile uint32_t _sim_spawn_rate = 35;
    volatile uint32_t _sim_reset_command = 0;

    // 128-bit UUIDs (Must match your GitHub index.html exactly)
    const ble_uuid128_t _gatt_svc_uuid = BLE_UUID128_INIT(
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50);

    const ble_uuid128_t _gatt_chr_uuid = BLE_UUID128_INIT(
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60);

    // GATT Service Tree
    struct ble_gatt_svc_def _gatt_svr_svcs[2];
    const struct ble_gatt_chr_def _bt_characteristics[2] = {
        {
            .uuid = &_gatt_chr_uuid.u,
            .access_cb = gatt_svr_write_callback,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        },
        {
            0, // No more characteristics (Terminator block)
        }};

    static Bluetooth *instance;

    friend int gatt_svr_write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    friend void nimble_host_task_wrapper(void *param);
    friend void ble_app_on_sync_wrapper(void);
};

#endif