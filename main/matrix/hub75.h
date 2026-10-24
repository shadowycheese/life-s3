#ifndef Hub75_h_
#define Hub75_h_

#include <driver/gpio.h>
#include <string.h>
#include "matrix/hub75_driver.h"

#define COL_BLACK 0x00
#define COL_RED 0xF00
#define COL_GREEN 0x0F0
#define COL_BLUE 0x00F
#define COL_CYAN (COL_GREEN | COL_BLUE)
#define COL_MAGENTA (COL_RED | COL_BLUE)
#define COL_YELLOW (COL_RED | COL_GREEN)
#define COL_WHITE (COL_RED | COL_BLUE | COL_GREEN)
#define COL_ORANGE 0xF80
#define COL_LIGHT_GREY 0xCCC
#define COL_DARK_GREY 0x444
#define RGB(r, g, b) ((((int)(r) & 0xF) << 8) | (((int)(g) & 0xF) << 4) | ((int)(b) & 0xF))

class Hub75
{
public:
    Hub75(Hub75Driver *driver)
    {
        _driver = driver;

        clear_screen();
    }

    inline int width()
    {
        return _driver->width();
    }

    inline int height()
    {
        return _driver->height();
    }

    void draw_pixel(int16_t x, int16_t y, uint16_t col)
    {
        if (_invert)
        {
            x = x & 0x1F;
            y = 0x3F - (y & 0x3F);

            _frame_buffer[(x * PANEL_WIDTH) + y] = col;
        }
        else
        {
            x = x & 0x3F;
            y = y & 0x1F;

            _frame_buffer[(y * PANEL_WIDTH) + x] = col;
        }
    }

    void commit_frame()
    {
        _driver->set_frame_buffer(&_frame_buffer[0]);
    }

    void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col)
    {
        for (int i = 0; i < w; i++)
        {
            for (int j = 0; j < h; j++)
            {
                draw_pixel(x + i, y + j, col);
            }
        }
    }

    void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col)
    {
        for (int i = 0; i < w; i++)
        {
            for (int j = 0; j < h; j++)
            {
                draw_pixel(x + i, y + j, col);
            }
        }
    }

    void clear_screen()
    {
        memset(&_frame_buffer[0], 0, PANEL_HEIGHT * PANEL_WIDTH);
    }

private:
    uint16_t _frame_buffer[PANEL_HEIGHT * PANEL_WIDTH * 2];
    bool _invert = false;
    Hub75Driver *_driver;
};

#endif