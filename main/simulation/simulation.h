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
#include "simulation/sim_model.h"

typedef struct
{
    char name[20];
    float feed_rate;
    float kill_rate;
} sim_preset_t;

typedef struct
{
    float A;
    float B;
} cell_t;

typedef struct
{
    uint8_t r, g, b;
} rgb_t;

typedef struct
{
    float r;
    float g;
    float b;
    float s;
} band_t;

class Simulation
{
public:
    void init(int width, int height)
    {
        _width = width;
        _height = height;
        _hue = 0;

        int array_size = _width * _height;

        size_t grid_bytes = sizeof(cell_t) * array_size;

        _grid = (cell_t *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);
        _temp_grid = (cell_t *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);

        reset();

        _spawn_chance = 200;
        _feed_rate = _presets[11].feed_rate;
        _kill_rate = _presets[11].kill_rate;

        srand(esp_random());
    }

    void set_configuration(sim_configuration_t *config)
    {
        if (config->magic == SIM_CONFIG_MAGIC)
        {
            _feed_rate = config->feed_rate;
            _kill_rate = config->kill_rate;
            _hue = config->hue % 6;
            _spawn_chance = config->spawn_chance;
        }
    }

    void get_configuration(sim_configuration_t *config)
    {
        config->feed_rate = _feed_rate;
        config->kill_rate = _kill_rate;
        config->spawn_chance = _spawn_chance;
        config->hue = _hue;
        config->magic = SIM_CONFIG_MAGIC;
    }

    inline int spawn_chance()
    {
        return _spawn_chance;
    }

    inline int set_spawn_chance(int spawn_chance)
    {
        _spawn_chance = spawn_chance;

        return _spawn_chance;
    }

    inline float set_feed_rate(float feed_rate)
    {
        _feed_rate = constrain(feed_rate, 0.0, 1.0);

        return _feed_rate;
    }

    inline float set_kill_rate(float kill_rate)
    {
        _kill_rate = constrain(kill_rate, 0.0, 1.0);

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

    inline void next_hue()
    {
        _hue = (_hue + 1) % 6;
    }

    inline void add(int16_t x, int16_t y)
    {
        _grid[ap(x, y)].B = 1.0f;
    }

    void reset()
    {
        int array_size = _width * _height;

        for (int i = 0; i < array_size; i++)
        {
            _grid[i] = {.A = 1.0, .B = 0.0};
            _temp_grid[i] = {.A = 1.0, .B = 0.0};
        }
    }

    const char *name()
    {
        for (int i = 0; i < 15; i++)
        {
            if ((_feed_rate == _presets[i].feed_rate) && (_kill_rate == _presets[i].kill_rate))
            {
                return _presets[i].name;
            }
        }

        return "";
    }

    int16_t preset()
    {
        for (int i = 0; i < 15; i++)
        {
            if ((_feed_rate == _presets[i].feed_rate) && (_kill_rate == _presets[i].kill_rate))
            {
                return i;
            }
        }

        return -1;
    }

    void set_preset(int preset)
    {
        preset = (preset) % 15;

        _feed_rate = _presets[preset].feed_rate;
        _kill_rate = _presets[preset].kill_rate;
    }

    void simulate()
    {
        if ((random() % 10000) < _spawn_chance)
        {
            spawn();
        }

        step_simulation();
    }

    uint16_t get_colour(int x, int y)
    {
        int p = ap(x, y);

        float l = constrain(1.0f - (_grid[p].A - _grid[p].B), 0.0f, 1.0f);

        return map_grayscott_rgb444(l) & 0xFFF;
    }

private:
    const sim_preset_t _presets[15] = {
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

    int _width;
    int _height;

    int _hue;

    cell_t *_grid;
    cell_t *_temp_grid;

    float _D_A = 1.0;
    float _D_B = 0.5;
    float _feed_rate;
    float _kill_rate;
    int16_t _spawn_chance;

    float interp(float from, float to, float val)
    {
        return (from + ((to - from) * val));
    }

    const band_t _bands[5] = {
        {.r = 0.0f, .g = 0.0f, .b = 0.0f, .s = 0.00f},
        {.r = 4.0f, .g = 1.0f, .b = 9.0f, .s = 0.40f},
        {.r = 9.0f, .g = 0.0f, .b = 13.0f, .s = 0.60f},
        {.r = 11.0f, .g = 3.0f, .b = 2.0f, .s = 0.80f},
        {.r = 9.0f, .g = 8.0f, .b = 1.0f, .s = 1.00f},
    };
    const float r0 = 0.0f, g0 = 0.0f, b0 = 0.0f;
    // Stage 1: purple/magenta
    const float r1 = 6.0f, g1 = 1.0f, b1 = 8.0f;
    // Stage 2: coral/orange
    const float r2 = 11.0f, g2 = 3.0f, b2 = 2.0f;
    // Stage 3: warm highlight / yellow
    const float r3 = 13.0f, g3 = 12.0f, b3 = 1.0f;

    const int _band_count = sizeof(_bands) / sizeof(band_t);

    uint16_t map_grayscott_rgb444(float val)
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        for (int i = 1; i < _band_count; i++)
        {
            if (val <= _bands[i].s)
            {
                float band_size = _bands[i].s - _bands[i - 1].s;
                float f = (val - _bands[i - 1].s) * (1.0f / band_size);

                r = interp(_bands[i - 1].r, _bands[i].r, f);
                g = interp(_bands[i - 1].g, _bands[i].g, f);
                b = interp(_bands[i - 1].b, _bands[i].b, f);

                break;
            }
        }

        uint16_t r_int = (uint16_t)(r + 0.5f);
        uint16_t g_int = (uint16_t)(g + 0.5f);
        uint16_t b_int = (uint16_t)(b + 0.5f);

        switch (_hue)
        {
        case 0:
            return RGB(r_int, g_int, b_int);
        case 1:
            return RGB(r_int, b_int, g_int);
        case 0:
            return RGB(g_int, r_int, b_int);
        case 1:
            return RGB(g_int, b_int, r_int);
        case 0:
            return RGB(b_int, r_int, g_int);
        case 1:
            return RGB(b_int, g_int, r_int);
        default:
            return RGB(r_int, g_int, b_int);
        }
    }

    inline void spawn()
    {
        _grid[random() % (_width * _height)].B = 1.0f;
    }

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
