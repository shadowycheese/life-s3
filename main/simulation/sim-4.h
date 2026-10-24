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
#include "simulation/sim.h"

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

class Simulation4 : public Simulation
{
public:
    Simulation4() {};

    void init(int width, int height)
    {
        _width = width;
        _height = height;

        int array_size = _width * _height;

        size_t grid_bytes = sizeof(cell_t) * array_size;

        _grid = (cell_t *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);
        _temp_grid = (cell_t *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);

        for (int i = 0; i < array_size; i++)
        {
            _grid[i] = {.A = 1.0, .B = 0.0};
            _temp_grid[i] = {.A = 1.0, .B = 0.0};
        }

        _param_index = 8;
        _feed_rate = _params[_param_index].feed_rate;
        _kill_rate = _params[_param_index].kill_rate;

        srand(esp_random());

        init_palette();
    }

    void simulate(double time)
    {
        if ((random() % 1000) == 0)
        {
            _param_index = (_param_index + 1) % 15;

            _feed_rate = _params[_param_index].feed_rate;
            _kill_rate = _params[_param_index].kill_rate;

            ESP_LOGI("SIM", "Changing parameters => F=%0.3f, K=%0.3f [%d]", _feed_rate, _kill_rate, _param_index);
        }

        if ((_init_count > 0) || (_weight < 0.10f) || ((random() % 32) == 0))
        {
            int p = random() % (_width * _height);

            _grid[p].B = 1.0f;

            _init_count--;
        }

        simulate();
    }

    inline float grid_level(int x, int y)
    {
        return _grid[(y * _width) + x].A;
    }

    uint16_t get_colour(int x, int y)
    {
        int p = ap(x, y);

        // float l = (_grid[p].A - _grid[p].B);

        // return 0xFFF - constrain(0xFFF * v, 0, 0xFFF);
        // uint16_t v2 = 0xFF - constrain(0xFF * l, 0, 0xFF);

        float l = constrain(1.0f - (_grid[p].A - _grid[p].B), 0.0f, 1.0f);

        float h = l * 360.0f;
        float s = constrain(l * 2.0f, 0.0f, 1.0f);
        float v = l / 2.0f;

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

        return ((((uint16_t)((r + m) * 31.0f)) << 7) & 0xF) |
               ((((uint16_t)((g + m) * 31.0f)) << 3) & 0xF) |
               ((((uint16_t)((b + m) * 31.0f)) >> 1) & 0xF);

        // float value =  1.0f - (_grid[p].A - _grid[p].B);
        // int shift = (int)(value * 6.0f);
        // int colour = ((int)(value * 64.0f)) << shift;
        //  index &= 0xFF;

        // return colour & 0xFFF;

        // return _palette[index];
    }

private:
    const parameters_t _params[15] = {
        {"Mitosos", 0.0367, 0.0649},       // Mitosis
        {"Coral Growth", 0.0545, 0.062},   // Coral Growth
        {"Fingerprint", 0.055, 0.062},     // Fingerprint
        {"Spirals", 0.018, 0.051},         // Spirals
        {"Worms", 0.078, 0.061},           // Worms
        {"Maze", 0.029, 0.057},            // Maze
        {"Bubbles", 0.098, 0.057},         // Bubbles
        {"Spots and Loops", 0.039, 0.058}, // Spots and Loops
        {"Waves", 0.014, 0.054},           // Waves
        {"Splotches", 0.026, 0.051},       // Splotches
        {"Solitions", 0.03, 0.06},         // Solitions
        {"Dots", 0.05, 0.065},             // Dots
        {"Stripes", 0.025, 0.06},          // Stripes
        {"Cross Waves", 0.012, 0.045},     // Cross Waves
        {"Mixed", 0.022, 0.051},           // Mixed
    };

    int _width;
    int _height;
    int _weight;

    int _init_count = 3;

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

    void simulate(void)
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
