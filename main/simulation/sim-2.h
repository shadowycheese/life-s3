#ifndef SIM2_H
#define SIM2_H

#include <stdio.h>
#include <stdlib.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <math.h>
#include "simulation/sim.h"

class Simulation2 : public Simulation
{
public:
    Simulation2() {};

    void init(int width, int height)
    {
        _width = width;
        _height = height;

        size_t grid_bytes = sizeof(float) * _width * _height;

        _grid1 = (float *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);
        _grid2 = (float *)(void *)heap_caps_aligned_alloc(4, grid_bytes, MALLOC_CAP_INTERNAL);

        memset(_grid1, 0, grid_bytes);
        memset(_grid2, 0, grid_bytes);

        srand(time(0));
        random_grid();
    }

    void simulate(double time)
    {
        // Swap buffers
        step_simulation();

        float *tmp = _grid1;
        _grid1 = _grid2;
        _grid2 = tmp;
    }

    inline float grid_level(int x, int y)
    {
        return _grid1[(y * _width) + x];
    }

private:
    int _width;
    int _height;
    float *_grid1;
    float *_grid2;
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
        _grid1[(y * _width) + x] = l;
    }

    void random_grid(void)
    {
        for (int y = 0; y < _height; y++)
        {
            for (int x = 0; x < _width; x++)
            {
                if (abs(y - _height / 2) < 15 && abs(x - _width / 2) < 15)
                {
                    set(x, y, rand_float());
                }
                else
                {
                    set(x, y, 0.0f);
                }
            }
        }
    }

    float growth_function(float total_neighbors)
    {
        float peak = 0.25f;  // Ideal neighborhood density
        float width = 0.05f; // Standard deviation of the bell curve

        // Gaussian distribution function
        return 2.0f * expf(-powf((total_neighbors - peak) / width, 2.0f)) - 1.0f;
    }

    void step_simulation()
    {
        // 3x3 Smoothing Kernel weights
        float kernel[3][3] = {
            {0.05f, 0.10f, 0.05f},
            {0.10f, 0.20f, 0.10f},
            {0.05f, 0.10f, 0.05f}};

        for (int y = 0; y < _height; y++)
        {
            for (int x = 0; x < _width; x++)
            {
                float neighborhood_sum = 0.0f;

                // Convolution over 3x3 neighborhood with toroidal (wrapping) boundaries
                for (int ky = -1; ky <= 1; ky++)
                {
                    for (int kx = -1; kx <= 1; kx++)
                    {
                        int ny = (y + ky + _height) % _height;
                        int nx = (x + kx + _width) % _width;
                        neighborhood_sum += _grid1[(ny * _width) + nx] * kernel[ky + 1][kx + 1];
                    }
                }

                // Calculate growth and update the state fluidly
                float growth = growth_function(neighborhood_sum);
                float dt = 0.1f; // Time step delta for smooth transition

                float new_val = _grid1[(y * _width) + x] + dt * growth;

                // Clamp value strictly between 0.0 (dead) and 1.0 (fully alive)
                if (new_val < 0.0f)
                {
                    new_val = 0.0f;
                }
                if (new_val > 1.0f)
                {
                    new_val = 1.0f;
                }

                _grid2[(y * _width) + x] = new_val;
            }
        }
    }
};
#endif
