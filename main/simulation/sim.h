#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

class Simulation
{
public:
    Simulation() {};

    virtual void init(int width, int height) = 0;

    virtual void simulate(double delta) = 0;

    virtual float grid_level(int x, int y) = 0;

    virtual uint16_t get_colour(int x, int y)
    {
        float value = grid_level(x, y);

        if (value <= 0.05f)
        {
            return 0;
        }

        uint8_t r = 0, g = 0, b = 0;

        if (value < 0.25f)
        {
            // Phase 1: Fade from Black to Deep Blue
            // Value range: 0.05 to 0.25. Maps to Blue: 1 to 15
            float t = (value - 0.05f) / 0.20f;
            b = (uint8_t)(t * 15.0f);
        }
        else if (value < 0.50f)
        {
            // Phase 2: Fade Blue into Deep Indigo/Purple
            // Blue stays high (15), Red ramps up from 0 to 12
            float t = (value - 0.25f) / 0.25f;
            r = (uint8_t)(t * 12.0f);
            b = 15;
        }
        else if (value < 0.80f)
        {
            // Phase 3: Transition Indigo into Bright Magenta/Hot Pink
            // Red hits maximum (15), Blue stays high (15), Green starts waking up (0 to 6)
            float t = (value - 0.50f) / 0.30f;
            r = 12 + (uint8_t)(t * 3.0f);
            g = (uint8_t)(t * 6.0f);
            b = 15;
        }
        else
        {
            // Phase 4: Core explosion! Cells fade into intense White centers
            // Red & Blue maxed (15), Green floods the rest of the way (6 to 15)
            float t = (value - 0.80f) / 0.20f;
            r = 15;
            g = 6 + (uint8_t)(t * 9.0f);
            b = 15;
        }

        // Use the native color444 converter provided by the driver/Adafruit GFX
        return ((((int)(r) & 0xF) << 8) | (((int)(g) & 0xF) << 4) | ((int)(b) & 0xF));
    }
};
#endif