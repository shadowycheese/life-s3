#ifndef SIM_MODEL_H
#define SIM_MODEL_H

#include <stdint.h>

#define SIM_CONFIG_MAGIC 0xFEEDD1ED

typedef struct
{
    float kill_rate;
    float feed_rate;
    int32_t spawn_chance;
    int32_t magic;
} sim_configuration_t;

typedef struct
{
    int16_t preset;
    float feed_rate;
    float kill_rate;
    int16_t spawn_chance;
} sim_state_t;

#endif
