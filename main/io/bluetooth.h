#ifndef Bluetooth_H
#define Bluetooth_H

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "esp_bt.h"
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
int ble_gap_event_handler_wrapper(struct ble_gap_event *event, void *arg);
void ble_app_on_sync_wrapper(void);

class Bluetooth
{
public:
    Bluetooth()
    {
        instance = this;
    };

    void init(void (*state_change)(int))
    {
        _gatt_svr_svcs[0] = {};
        _gatt_svr_svcs[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
        _gatt_svr_svcs[0].uuid = &_gatt_svc_uuid.u;
        _gatt_svr_svcs[0].characteristics = _bt_characteristics;
        _gatt_svr_svcs[1] = {0}; // No more services

        nimble_port_init();
        ble_hs_cfg.sync_cb = ble_app_on_sync_wrapper;

        _state_change = state_change;

        // 3. Initialize custom GATT tables
        ble_svc_gap_init();
        ble_svc_gatt_init();
        ble_gatts_count_cfg(_gatt_svr_svcs);
        ble_gatts_add_svcs(_gatt_svr_svcs);
        // esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P3);

        // Set default network name inside GAP
        ble_svc_gap_device_name_set("Cells-Remote");

        // 4. Spin up the host task loop
        // (Note: Task pinning here is a safety fallback; the 'menuconfig' setting dominates affinity)
        xTaskCreatePinnedToCore(nimble_host_task_wrapper, "nimble_host", 4096, NULL, 10, NULL, 0);
    }

    bool active()
    {
        return _bt_active;
    }

    bool kill_rate_updated(int16_t *kill_rate)
    {
        if (_bt_kill_rate >= 0)
        {
            *kill_rate = _bt_kill_rate;

            _bt_kill_rate = -1;

            return true;
        }

        return false;
    }

    bool feed_rate_updated(int16_t *feed_rate)
    {
        if (_bt_feed_rate >= 0)
        {
            *feed_rate = _bt_feed_rate;

            _bt_feed_rate = -1;

            return true;
        }

        return false;
    }

    bool spawn_chance_updated(int16_t *spawn_chance)
    {
        if (_bt_spawn_chance >= 0)
        {
            *spawn_chance = _bt_spawn_chance;

            _bt_spawn_chance = -1;

            return true;
        }

        return false;
    }

    bool preset_updated(int16_t *preset)
    {
        if (_bt_preset >= 0)
        {
            *preset = _bt_preset;

            _bt_preset = -1;

            return true;
        }

        return false;
    }

    bool reset_command()
    {
        if (_bt_reset)
        {
            _bt_reset = false;

            return true;
        }

        return false;
    }

    bool add_command(int16_t *add)
    {
        if (_bt_add >= 0)
        {
            *add = _bt_add;

            _bt_add = -1;

            return true;
        }

        return false;
    }

private:
    int read_write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            return 0;
        }
        else if (ctxt->om->om_len >= 2)
        {
            uint8_t command = ctxt->om->om_data[0];
            int16_t value = ctxt->om->om_data[1];

            if (ctxt->om->om_len > 2)
            {
                value *= 256;
                value += ctxt->om->om_data[2];
            }

            ESP_LOGI(TAG, "New command parsed on Core 1: %d, %d", command, value);

            switch (command)
            {
            case 0x41:
                _bt_feed_rate = value;
                break;
            case 0x42:
                _bt_kill_rate = value;
                break;
            case 0x43:
                _bt_spawn_chance = value;
                break;
            case 0x44:
                _bt_preset = value;
                break;
            case 0x45:
                _bt_add = value;
                break;
            case 0x46:
                _bt_reset = true;
                break;
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
        // adv_params.channel_map = 0x01;
        ble_gap_adv_start(_ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event_handler_wrapper, NULL);
    }

    int handle_gap_event(struct ble_gap_event *event)
    {
        switch (event->type)
        {
        case BLE_GAP_EVENT_CONNECT:
            _bt_active = true;
            //_state_change(1);
            ESP_LOGI("BLE", "Phone Connected! Status: %d", event->connect.status);
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI("BLE", "Phone Disconnected! Reason: %d. Restarting radio scan...", event->disconnect.reason);
            //_state_change(0);
            _bt_active = false;
            ble_app_advertise();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI("BLE", "Advertising cycle completed naturally. Restarting scan...");
            ble_app_advertise();
            break;

        default:
            break;
        }

        return ESP_OK;
    }

    void ble_app_on_sync(void)
    {
        ESP_LOGI(TAG, "ble_hs_id_infer_auto");
        ble_hs_id_infer_auto(0, &_ble_addr_type);
        ESP_LOGI(TAG, "ble_app_advertise: %d", _ble_addr_type);
        ble_app_advertise();
        ESP_LOGI(TAG, "ble_app_on_sync complete");
    }

    // Dedicated FreeRTOS background task running host execution state
    void nimble_host_task()
    {
        nimble_port_run();
        nimble_port_freertos_deinit();
    }

    static constexpr char *TAG = "BLE_CTRL";

    uint8_t _ble_addr_type;

    // Your atomic cross-core global matrix variables
    volatile int16_t _bt_feed_rate = -1;
    volatile int16_t _bt_kill_rate = -1;
    volatile int16_t _bt_preset = -1;
    volatile int16_t _bt_spawn_chance = -1;
    volatile bool _bt_reset = false;
    volatile int16_t _bt_add = -1;
    volatile bool _bt_active = false;

    void (*_state_change)(int);

    const ble_uuid128_t _gatt_svc_uuid = BLE_UUID128_INIT(
        0x50, 0x4F, 0x4E, 0x4D, 0x4C, 0x4B, 0x4A, 0x49,
        0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41);

    const ble_uuid128_t _gatt_chr_uuid = BLE_UUID128_INIT(
        0x60, 0x5F, 0x5E, 0x5D, 0x5C, 0x5B, 0x5A, 0x59,
        0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51);

    // GATT Service Tree
    struct ble_gatt_svc_def _gatt_svr_svcs[2];
    const struct ble_gatt_chr_def _bt_characteristics[2] = {
        {
            .uuid = &_gatt_chr_uuid.u,
            .access_cb = gatt_svr_write_callback,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_ACCESS_OP_READ_CHR,
        },
        {
            0, // No more characteristics (Terminator block)
        }};

    static Bluetooth *instance;

    friend int gatt_svr_write_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    friend void nimble_host_task_wrapper(void *param);
    friend void ble_app_on_sync_wrapper(void);
    friend int ble_gap_event_handler_wrapper(struct ble_gap_event *event, void *arg);
};

#endif