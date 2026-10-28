#ifndef Encoder_H
#define Encoder_H

#include <stdint.h>
#include "driver/gpio.h"
#include "io/espio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>

#define FEED_RATE_ENCODER 0
#define KILL_RATE_ENCODER 1

#define FEED_RATE_ENCODER_PIN_A GPIO_NUM_38
#define FEED_RATE_ENCODER_PIN_B GPIO_NUM_37
#define FEED_RATE_ENCODER_BUTTON_PIN GPIO_NUM_36

#define KILL_RATE_ENCODER_PIN_A GPIO_NUM_47
#define KILL_RATE_ENCODER_PIN_B GPIO_NUM_16
#define KILL_RATE_ENCODER_BUTTON_PIN GPIO_NUM_35

void spin_feed_rate_isr(void *);
void spin_kill_rate_isr(void *);
void setup_encoders(void *);

typedef enum
{
    BUTTON_NOT_CLICKED,
    BUTTON_CLICKED,
    BUTTON_HELD,
} click_type_t;

#define CLICK_DEBOUNCE (100LL * 1000LL)
#define HOLD_DEBOUCE (2000LL * 1000LL)
#define HOLD_REPEAT (250LL * 1000LL)
#define CLICK_MAX (HOLD_DEBOUCE - HOLD_REPEAT)

class RotaryEncoders
{
public:
    RotaryEncoders()
    {
    }

    void init()
    {
        instance = this;

        xTaskCreatePinnedToCore(
            setup_encoders,
            "encoder_init",
            2048,
            NULL,
            5,
            NULL,
            1);

        while (!_ready)
        {
            vTaskDelay(1);
        }

        ESP_LOGI("ENCODERS", "Encoder ISR's registered");
    }

    void setup_encoder_isrs()
    {
        gpio_install_isr_service(0);

        configure_input_pin(FEED_RATE_ENCODER_PIN_A, GPIO_INTR_ANYEDGE);
        configure_input_pin(FEED_RATE_ENCODER_PIN_B, GPIO_INTR_ANYEDGE);
        configure_input_pin(KILL_RATE_ENCODER_PIN_A, GPIO_INTR_ANYEDGE);
        configure_input_pin(KILL_RATE_ENCODER_PIN_B, GPIO_INTR_ANYEDGE);

        configure_input_pin(FEED_RATE_ENCODER_BUTTON_PIN, GPIO_INTR_DISABLE);
        configure_input_pin(KILL_RATE_ENCODER_BUTTON_PIN, GPIO_INTR_DISABLE);

        _state[FEED_RATE_ENCODER] = (gpio_get_level(FEED_RATE_ENCODER_PIN_A) << 1) | gpio_get_level(FEED_RATE_ENCODER_PIN_B);
        _state[KILL_RATE_ENCODER] = (gpio_get_level(KILL_RATE_ENCODER_PIN_A) << 1) | gpio_get_level(KILL_RATE_ENCODER_PIN_B);

        gpio_isr_handler_add(FEED_RATE_ENCODER_PIN_A, spin_feed_rate_isr, (void *)FEED_RATE_ENCODER);
        gpio_isr_handler_add(FEED_RATE_ENCODER_PIN_B, spin_feed_rate_isr, (void *)FEED_RATE_ENCODER);

        gpio_isr_handler_add(KILL_RATE_ENCODER_PIN_A, spin_kill_rate_isr, (void *)KILL_RATE_ENCODER);
        gpio_isr_handler_add(KILL_RATE_ENCODER_PIN_B, spin_kill_rate_isr, (void *)KILL_RATE_ENCODER);

        _ready = true;
    }

    inline int get_and_reset_feed_rate()
    {
        return get_and_reset(FEED_RATE_ENCODER);
    }

    inline int get_and_reset_kill_rate()
    {
        return get_and_reset(KILL_RATE_ENCODER);
    }

    inline click_type_t is_feed_rate_clicked(uint64_t now)
    {
        return is_clicked(FEED_RATE_ENCODER, FEED_RATE_ENCODER_BUTTON_PIN, now);
    }

    inline click_type_t is_kill_rate_clicked(uint64_t now)
    {
        return is_clicked(KILL_RATE_ENCODER, KILL_RATE_ENCODER_BUTTON_PIN, now);
    }

private:
    inline click_type_t is_clicked(int encoder, gpio_num_t button_pin, uint64_t now)
    {
        if (gpio_get_level(button_pin) == 0)
        {
            if (_button_down_time[encoder] == 0)
            {
                _button_down_time[encoder] = now;
            }
            else
            {
                uint64_t hold_time = now - _button_down_time[encoder];

                if (hold_time > HOLD_DEBOUCE)
                {
                    _button_down_time[encoder] += HOLD_REPEAT;

                    printf("HELD\n");
                    return BUTTON_HELD;
                }
            }
        }
        else if (_button_down_time[encoder] != 0)
        {
            uint64_t diff = now - _button_down_time[encoder];

            _button_down_time[encoder] = 0;

            if ((diff > CLICK_DEBOUNCE) && (diff < CLICK_MAX))
            {
                printf("CLICKED\n");
                return BUTTON_CLICKED;
            }
        }

        return BUTTON_NOT_CLICKED;
    }

    inline int get_and_reset(int encoder)
    {
        int detente = _detente[encoder];

        _detente[encoder] = 0;

        return detente;
    }

    void update_encoder(int encoder, gpio_num_t pin_a, gpio_num_t pin_b)
    {
        static const int8_t table[16] =
            {
                0, -1, 1, 0,
                1, 0, 0, -1,
                -1, 0, 0, 1,
                0, 1, -1, 0};

        int new_state = (gpio_get_level(pin_a) << 1) | gpio_get_level(pin_b);

        if (new_state == _state[encoder])
        {
            // return;
        }

        uint8_t index = (_state[encoder] << 2) | new_state;

        _delta[encoder] += table[index];

        if (_delta[encoder] >= 4)
        {
            _detente[encoder] = _detente[encoder] + 1;
            _delta[encoder] = 0;
        }
        else if (_delta[encoder] <= -4)
        {
            _detente[encoder] = _detente[encoder] - 1;
            _delta[encoder] = 0;
        }

        _state[encoder] = new_state;
    }

    volatile bool _ready = false;
    int _debounce[2] = {2, 20};
    uint64_t _button_down_time[2];
    int8_t _delta[2] = {0, 0};
    uint8_t _state[2] = {0, 0};

    volatile int _detente[2] = {0, 0};

    static RotaryEncoders *instance;

    friend void spin_feed_rate_isr(void *);
    friend void spin_kill_rate_isr(void *);
    friend void setup_encoders(void *);
};

#endif