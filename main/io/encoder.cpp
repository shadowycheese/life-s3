#include "io/encoder.h"

RotaryEncoders *RotaryEncoders::instance = nullptr;

void IRAM_ATTR spin_feed_rate_isr(void *)
{
    RotaryEncoders::instance->update_encoder(FEED_RATE_ENCODER, FEED_RATE_ENCODER_PIN_A, FEED_RATE_ENCODER_PIN_B);
}

void IRAM_ATTR spin_kill_rate_isr(void *)
{
    RotaryEncoders::instance->update_encoder(KILL_RATE_ENCODER, KILL_RATE_ENCODER_PIN_A, KILL_RATE_ENCODER_PIN_B);
}

void setup_encoders(void *)
{
    RotaryEncoders::instance->setup_encoder_isrs();

    vTaskDelete(NULL);
}