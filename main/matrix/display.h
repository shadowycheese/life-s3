#ifndef Display_h_
#define Display_h_

#include <driver/gpio.h>
#include <string.h>
#include <esp_heap_caps.h>
#include "matrix/hub75_driver.h"
#include "matrix/gfx/Adafruit_GFX.h"

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

class Display : public Adafruit_GFX
{
public:
    Display(Hub75Driver *driver) : Adafruit_GFX(driver->width(), driver->height())
    {
        _driver = driver;
        _width = driver->width();
        _height = driver->height();
    }

    void init()
    {
        size_t buf_size = _width * _height * sizeof(uint16_t);

        _frame_buffer = (uint16_t *)heap_caps_aligned_alloc(4, buf_size, MALLOC_CAP_DEFAULT);

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

    void drawPixel(int16_t x, int16_t y, uint16_t col)
    {
        draw_pixel(x, y, col);
    }

    uint16_t get_pixel(int16_t x, int16_t y)
    {
        return _frame_buffer[(y * PANEL_WIDTH) + x];
    }

    void draw_pixel(int16_t x, int16_t y, uint16_t col)
    {
        x = x & 0x3F;
        y = y & 0x1F;

        _frame_buffer[(y * PANEL_WIDTH) + x] = col;
    }

    void commit_frame()
    {
        _driver->set_frame_buffer(&_frame_buffer[0]);
    }

    void clear_screen()
    {
        memset(&_frame_buffer[0], 0, _width * _height);
    }

private:
    uint16_t *_frame_buffer;

    int _width;
    int _height;

    Hub75Driver *_driver;
};

#endif