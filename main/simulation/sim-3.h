#ifndef SIM3_H
#define SIM3_H

#include <stdio.h>
#include <stdlib.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <math.h>
#include "simulation/sim.h"

class Simulation3 : public Simulation
{
public:
    Simulation3() {};

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
        int p = rand() % (_width * _height);

        _grid1[p] = rand_float();

        p = rand() % (_width * _height);

        _grid1[p] = 0;

        // Swap buffers
        step_lenia_simulation();

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
                set(x, y, rand_float());
            }
        }
    }

    const float lenia_kernel[7][7] = {
        {0.000f, 0.005f, 0.012f, 0.015f, 0.012f, 0.005f, 0.000f},
        {0.005f, 0.022f, 0.035f, 0.038f, 0.035f, 0.022f, 0.005f},
        {0.012f, 0.035f, 0.010f, 0.000f, 0.010f, 0.035f, 0.012f},
        {0.015f, 0.038f, 0.000f, 0.000f, 0.000f, 0.038f, 0.015f},
        {0.012f, 0.035f, 0.010f, 0.000f, 0.010f, 0.035f, 0.012f},
        {0.005f, 0.022f, 0.035f, 0.038f, 0.035f, 0.022f, 0.005f},
        {0.000f, 0.005f, 0.012f, 0.015f, 0.012f, 0.005f, 0.000f}};

    // --- THE CRITICAL MATHEMATICAL CONSTANTS ---
    // These specific figures are required to sustain stable, moving digital life.
    const float GROWTH_MEAN = 0.150f;  // Center of the ideal neighbor density
    const float GROWTH_SIGMA = 0.015f; // Sharpness of the growth window (very narrow)
    const float TIME_STEP_DT = 0.080f; // Lower dt forces physics to transition smoothly

    // Lenia Gaussian Growth Function mapping
    float lenia_growth(float neighborhood_sum)
    {
        // Standard Gaussian curve shifted between -1.0 and +1.0
        float exponent = powf((neighborhood_sum - GROWTH_MEAN) / GROWTH_SIGMA, 2.0f);
        return (2.0f * expf(-0.5f * exponent)) - 1.0f;
    }

    // Upgraded Simulation Loop (Optimized for ESP32-S3 execution)
    void step_lenia_simulation()
    {
        for (int y = 0; y < _height; y++)
        {
            for (int x = 0; x < _width; x++)
            {
                float U = 0.0f; // Total neighborhood weight accumulation

                // 7x7 Toroidal Window Convolution
                for (int ky = -3; ky <= 3; ky++)
                {
                    int ny = (y + ky + _height) % _height; // Edge wrap-around

                    for (int kx = -3; kx <= 3; kx++)
                    {
                        int nx = (x + kx + _width) % _width; // Edge wrap-around

                        // Multiply nearby cell value by the ring kernel weight
                        U += _grid1[(ny * _width) + nx] * lenia_kernel[ky + 3][kx + 3];
                    }
                }

                // Calculate evolutionary delta using the narrow growth window
                float delta = lenia_growth(U);

                // Apply fluid differential update
                float new_state = _grid1[(y * _width) + x] + (TIME_STEP_DT * delta);

                // Strict hardware clamping boundary [0.0, 1.0]
                if (new_state < 0.0f)
                    new_state = 0.0f;
                else if (new_state > 1.0f)
                    new_state = 1.0f;

                _grid2[(y * _width) + x] = new_state;
            }
        }
    }
};
#endif
