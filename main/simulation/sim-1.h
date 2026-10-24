#ifndef SIM1_H
#define SIM1_H

#include <stdio.h>
#include <stdlib.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <math.h>
#include "simulation/sim.h"

class Simulation1 : public Simulation
{
public:
    Simulation1() {};

    void init(int width, int height)
    {
        _width = width;
        _height = height;

        size_t grid_bytes = sizeof(float) * _width * _height;

        _grid = (float *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);
        _grid_diff = (float *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);

        memset(_grid, 0, grid_bytes);
        memset(_grid_diff, 0, grid_bytes);

        srand(time(0));
        random_grid();
    }

    void simulate(double delta)
    {
        compute_grid_diff();
        apply_grid_diff();
    }

    inline float grid_level(int x, int y)
    {
        return _grid[(y * _width) + x];
    }

private:
    int _width;
    int _height;
    float *_grid;
    float *_grid_diff;
    float _ra = 11;
    float _alpha_n = 0.028;
    float _alpha_m = 0.147;
    float _b1 = 0.278;
    float _b2 = 0.365;
    float _d1 = 0.267;
    float _d2 = 0.445;
    float _dt = 0.05f;

    inline float rand_float(void)
    {
        return (float)rand() / (float)RAND_MAX;
    }

    inline void set(int x, int y, float l)
    {
        _grid[(y * _width) + x] = l;
    }

    void random_grid(void)
    {
        int w = _width / 3;
        int h = _height / 3;

        for (int dy = 0; dy < h; dy++)
        {
            for (int dx = 0; dx < w; dx++)
            {
                int x = dx + _width / 2 - w / 2;
                int y = dy + _height / 2 - h / 2;

                _grid[(y * _width) + x] = rand_float();
            }
        }
    }

    inline int emod(int a, int b)
    {
        return (a % b + b) % b;
    }

    inline float sigma(float x, float a, float alpha)
    {
        return 1.0f / (1.0f + expf(-(x - a) * 4 / alpha));
    }

    inline float sigma_n(float x, float a, float b)
    {
        return sigma(x, a, _alpha_n) * (1 - sigma(x, b, _alpha_n));
    }

    inline float sigma_m(float x, float y, float m)
    {
        return x * (1 - sigma(m, 0.5f, _alpha_m)) + y * sigma(m, 0.5f, _alpha_m);
    }

    inline float s(float n, float m)
    {
        return sigma_n(n, sigma_m(_b1, _d1, m), sigma_m(_b2, _d2, m));
    }

    void compute_grid_diff(void)
    {
        for (int cy = 0; cy < _height; cy++)
        {
            for (int cx = 0; cx < _width; cx++)
            {
                float m = 0, M = 0;
                float n = 0, N = 0;
                float ri = _ra / 3;

                for (int dy = -(_ra - 1); dy <= (_ra - 1); dy++)
                {
                    for (int dx = -(_ra - 1); dx <= (_ra - 1); dx++)
                    {
                        int x = emod(cx + dx, _width);
                        int y = emod(cy + dy, _height);
                        if ((dx * dx) + (dy * dy) <= (ri * ri))
                        {
                            m += grid_level(x, y);
                            M += 1;
                        }
                        else if ((dx * dx) + (dy * dy) <= (_ra * _ra))
                        {
                            n += grid_level(x, y);
                            N += 1;
                        }
                    }
                }
                m /= M;
                n /= N;
                float q = s(n, m);
                _grid_diff[(cy * _width) + cx] = 2 * q - 1;
            }
        }
    }

    void clamp(float *x, float l, float h)
    {
        if (*x < l)
        {
            *x = l;
        }
        if (*x > h)
        {
            *x = h;
        }
    }

    void apply_grid_diff(void)
    {
        for (size_t y = 0; y < _height; ++y)
        {
            for (size_t x = 0; x < _width; ++x)
            {
                int p = (y * _width) + x;

                _grid[p] += _dt * _grid_diff[p];

                clamp(&_grid[p], 0, 1);
            }
        }
    }
};
#endif