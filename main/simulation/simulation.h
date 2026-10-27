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
#define CHANCE_PER_10K (100)

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

    inline int chance_adjust()
    {
        _chance_index = (_chance_index + 1) % 10;

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

    uint16_t get_colour(int x, int y)
    {
        int p = ap(x, y);

        float l = constrain(1.0f - (_grid[p].A - _grid[p].B), 0.0f, 1.0f);

        uint32_t v2 = ((int)(0xFFF * l)) & 0xFFF;

        if (v2 == 0)
        {
            return 0;
        }

        int lz = __builtin_clz(v2);

        if (lz >= 28)
        {
            return v2;
        }

        uint32_t mk = (0xFU << (28 - lz));
        uint16_t v3 = v2 & mk;

        return (uint16_t)v3;

        float h = l * 360.0f;
        float s = constrain(l * 2.0f, 0.0f, 1.0f);
        float v = l / 4.0f;

        float c = v * s;
        float n = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        float r, g, b;

        if (h < 60)
        {
            r = c;
            g = n;
            b = 0;
        }
        else if (h < 120)
        {
            r = n;
            g = c;
            b = 0;
        }
        else if (h < 180)
        {
            r = 0;
            g = c;
            b = n;
        }
        else if (h < 240)
        {
            r = 0;
            g = n;
            b = c;
        }
        else if (h < 300)
        {
            r = n;
            g = 0;
            b = c;
        }
        else
        {
            r = c;
            g = 0;
            b = n;
        }

        uint16_t rr = ((uint16_t)((r + m) * 15.0f)) << 8;
        uint16_t gg = ((uint16_t)((g + m) * 15.0f)) << 4;
        uint16_t bb = ((uint16_t)((b + m) * 15.0f));

        uint16_t c2 = (rr & 0xF00) | (gg & 0x0F0) | (bb & 0x00F);

        return c2;
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

    const int _chances[10] =
        {
            1,
            10,
            20,
            50,
            100,
            200,
            500,
            1000,
            2000,
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
        float weight = 0.0f;

        for (int i = 0; i < as; i++)
        {
            _grid[i].A = _temp_grid[i].A;
            _grid[i].B = _temp_grid[i].B;

            weight += 1.0 - constrain(_grid[i].A - _grid[i].B, 0, 1);

            _temp_grid[i].A = 1.0f;
            _temp_grid[i].B = 0.0f;
        }

        _weight = weight;
    }
};
#endif
