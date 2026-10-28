#ifndef SIM4_H
#define SIM4_H

/*
Pattern        | f       | k
---------------|---------|---------
Mitosis        | 0.0367  | 0.0649
Coral Growth   | 0.0545  | 0.062
Fingerprint    | 0.055   | 0.062
Spirals        | 0.018   | 0.051
Worms          | 0.078   | 0.061
Maze           | 0.029   | 0.057
Bubbles        | 0.098   | 0.057
Spots and Loops| 0.039   | 0.058
Waves          | 0.014   | 0.054
Splotches      | 0.026   | 0.051
Solitions      | 0.03    | 0.06
Dots           | 0.05    | 0.065
Stripes        | 0.025   | 0.06
Cross Waves    | 0.012   | 0.045
Mixed          | 0.022   | 0.051
*/

#include <stdio.h>
#include <stdlib.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <math.h>
#include <esp_random.h>

#define INITIAL_COUNT (3)

#define SIM_CONFIG_MAGIC 0xFEEDD1ED

typedef struct
{
    float kill_rate;
    float feed_rate;
    int32_t seed_chance;
    int32_t magic;
} sim_configuration_t;

typedef struct
{
    char name[20];
    float feed_rate;
    float kill_rate;
} parameters_t;

typedef struct
{
    float A;
    float B;
} cell_t;

typedef struct
{
    uint8_t r, g, b;
} rgb_t;

class Simulation
{
public:
    void init(int width, int height)
    {
        _width = width;
        _height = height;

        _init_count = INITIAL_COUNT;

        int array_size = _width * _height;

        size_t grid_bytes = sizeof(cell_t) * array_size;

        _grid = (cell_t *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);
        _temp_grid = (cell_t *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);

        for (int i = 0; i < array_size; i++)
        {
            _grid[i] = {.A = 1.0, .B = 0.0};
            _temp_grid[i] = {.A = 1.0, .B = 0.0};
        }

        _chance_index = 4;
        _param_index = 11;
        _feed_rate = _params[_param_index].feed_rate;
        _kill_rate = _params[_param_index].kill_rate;

        srand(esp_random());
    }

    void set_configuration(sim_configuration_t *config)
    {
        if (config->magic == SIM_CONFIG_MAGIC)
        {
            _feed_rate = config->feed_rate;
            _kill_rate = config->kill_rate;
            _chance_index = config->seed_chance % 22;
        }
    }

    void get_configuration(sim_configuration_t *config)
    {
        config->feed_rate = _feed_rate;
        config->kill_rate = _kill_rate;
        config->seed_chance = _chance_index;
        config->magic = SIM_CONFIG_MAGIC;
    }

    inline int chance_adjust()
    {
        _chance_index = (_chance_index + 1) % 22;

        return _chances[_chance_index];
    }

    inline int chance()
    {
        return _chances[_chance_index];
    }

    inline float increment_feed_rate(float feed_rate_step)
    {
        _feed_rate = increment(_feed_rate, feed_rate_step);

        return _feed_rate;
    }

    inline float increment_kill_rate(float kill_rate_step)
    {
        _kill_rate = increment(_kill_rate, kill_rate_step);

        return _kill_rate;
    }

    inline float kill_rate()
    {
        return _kill_rate;
    }

    inline float feed_rate()
    {
        return _feed_rate;
    }

    inline float increment(float current, float delta)
    {
        current += delta;

        if (current > 0.1f)
        {
            current = 0.1f;
        }
        else if (current < 0.001f)
        {
            current = 0.001f;
        }

        return current;
    }

    const char *name()
    {
        for (int i = 0; i < 15; i++)
        {
            if ((_feed_rate == _params[i].feed_rate) && (_kill_rate == _params[i].kill_rate))
            {
                return _params[i].name;
            }
        }

        return "";
    }

    int next_preset()
    {
        _param_index = (_param_index + 1) % 15;

        _feed_rate = _params[_param_index].feed_rate;
        _kill_rate = _params[_param_index].kill_rate;

        return _param_index;
    }

    int add()
    {
        _init_count = 1;

        return _param_index;
    }

    void simulate()
    {
        if ((_init_count > 0) || (_weight < -0.10f) || ((random() % 10000) < _chances[_chance_index]))
        {
            int p = random() % (_width * _height);

            _grid[p].B = 1.0f;

            _init_count--;
        }

        step_simulation();
    }

    uint16_t map_grayscott_rgb444(float val)
    {
        // Stage 0: black (background/quiet zone)
        const float r0 = 0.0f, g0 = 0.0f, b0 = 0.0f;
        // Stage 1: purple/magenta
        const float r1 = 6.0f, g1 = 1.0f, b1 = 8.0f;
        // Stage 2: coral/orange
        const float r2 = 13.0f, g2 = 4.0f, b2 = 2.0f;
        // Stage 3: warm highlight / yellow
        const float r3 = 13.0f, g3 = 12.0f, b3 = 1.0f;

        float r, g, b;

        // 2. Piecewise linear interpolation across 3 segments
        if (val < 0.333333f)
        {
            float f = val * 3.0f;
            r = r0 + f * (r1 - r0);
            g = g0 + f * (g1 - g0);
            b = b0 + f * (b1 - b0);
        }
        else if (val < 0.666667f)
        {
            float f = (val - 0.333333f) * 3.0f;
            r = r1 + f * (r2 - r1);
            g = g1 + f * (g2 - g1);
            b = b1 + f * (b2 - b1);
        }
        else
        {
            float f = (val - 0.666667f) * 3.0f;
            r = r2 + f * (r3 - r2);
            g = g2 + f * (g3 - g2);
            b = b2 + f * (b3 - b2);
        }

        uint16_t r_int = (uint16_t)(r + 0.5f);
        uint16_t g_int = (uint16_t)(g + 0.5f);
        uint16_t b_int = (uint16_t)(b + 0.5f);

        return RGB(r_int, g_int, b_int);
    }

    uint16_t get_colour(int x, int y)
    {
        int p = ap(x, y);

        // float l = constrain(1.0f - (_grid[p].A - _grid[p].B), 0.0f, 1.0f);
        float l = constrain(1.0f - (_grid[p].A - _grid[p].B), 0.0f, 1.0f);

        return map_grayscott_rgb444(l);
    }

private:
    const parameters_t _params[15] = {
        {"Mitosos", 0.0367, 0.0649}, // Mitosis
        {"Coral", 0.0545, 0.062},    // Coral Growth
        {"Fingerpr.", 0.055, 0.062}, // Fingerprint
        {"Spiral", 0.018, 0.051},    // Spirals
        {"Worm", 0.078, 0.061},      // Worms
        {"Maze", 0.029, 0.057},      // Maze
        {"Bubbles", 0.098, 0.057},   // Bubbles
        {"Spot/Loop", 0.039, 0.058}, // Spots and Loops
        {"Waves", 0.014, 0.054},     // Waves
        {"Splotch", 0.026, 0.051},   // Splotches
        {"Solition", 0.03, 0.06},    // Solitions
        {"Dot", 0.05, 0.065},        // Dots
        {"Stripe", 0.025, 0.06},     // Stripes
        {"X/Wave", 0.012, 0.045},    // Cross Waves
        {"Mixed", 0.022, 0.051},     // Mixed
    };

    const int _chances[22] =
        {
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
            5000,
    };

    int _width;
    int _height;

    int _weight;
    int _param_index;
    int _chance_index;
    int _init_count;

    cell_t *_grid;
    cell_t *_temp_grid;

    float _D_A = 1.0;
    float _D_B = 0.5;
    float _feed_rate;
    float _kill_rate;

    inline int ap(int x, int y)
    {
        return (y * _width) + x;
    }

    inline int emod(int a, int b)
    {
        return (a % b + b) % b;
    }

    double constrain(double value, double low, double high)
    {
        return value > high ? high : (value < low ? low : value);
    }

    float laplacian_func(int x, int y, bool type)
    {
        int left = emod(x - 1, _width);
        int right = emod(x + 1, _width);
        int up = emod(y - 1, _height);
        int down = emod(y + 1, _height);

        return (type ? (0.2 * (_grid[ap(right, y)].A + _grid[ap(left, y)].A + _grid[ap(x, down)].A + _grid[ap(x, up)].A) +
                        0.05 * (_grid[ap(left, up)].A + _grid[ap(right, up)].A + _grid[ap(left, down)].A + _grid[ap(right, down)].A) - _grid[ap(x, y)].A)
                     : (0.2 * (_grid[ap(right, y)].B + _grid[ap(left, y)].B + _grid[ap(x, down)].B + _grid[ap(x, up)].B) +
                        0.05 * (_grid[ap(left, up)].B + _grid[ap(right, up)].B + _grid[ap(left, down)].B + _grid[ap(right, down)].B) - _grid[ap(x, y)].B));
    }

    void step_simulation()
    {
        for (int x = 0; x < _width; x++)
        {
            for (int y = 0; y < _height; y++)
            {
                int p = ap(x, y);

                _temp_grid[p].A = constrain(_grid[p].A + (_D_A * laplacian_func(x, y, true) -
                                                          _grid[p].A * _grid[p].B * _grid[p].B + _feed_rate * (1 - _grid[p].A)),
                                            0, 1);
                _temp_grid[p].B = constrain(_grid[p].B + (_D_B * laplacian_func(x, y, false) +
                                                          _grid[p].A * _grid[p].B * _grid[p].B - (_kill_rate + _feed_rate) * _grid[p].B),
                                            0, 1);
            }
        }

        int as = _width * _height;

        for (int i = 0; i < as; i++)
        {
            _grid[i].A = _temp_grid[i].A;
            _grid[i].B = _temp_grid[i].B;
        }
    }
};
#endif
