/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#include <string.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "pico/st7789.h"

static struct st7789_config st7789_cfg;
static uint16_t st7789_width;
static uint16_t st7789_height;
static bool st7789_data_mode = false;

#ifndef ST7789_SPI_BAUDRATE
#define ST7789_SPI_BAUDRATE (10u * 1000u * 1000u)
#endif

static void loop_delay(void)
{
       int x = 0;
       int y = 0;

       for(x=0 ; x<50 ; x++)
       {
               y++;
       }
}

static void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len)
{
    if (st7789_cfg.gpio_cs > -1) {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }
    st7789_data_mode = false;

    loop_delay();
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
    gpio_put(st7789_cfg.gpio_dc, 0);
    loop_delay();
    
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));
    
    if (len) {
        loop_delay();
        gpio_put(st7789_cfg.gpio_dc, 1);
        loop_delay();
        
        spi_write_blocking(st7789_cfg.spi, data, len);
    }

    loop_delay();
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 1);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    loop_delay();
}

void st7789_caset(uint16_t xs, uint16_t xe)
{
    uint8_t data[] = {
        xs >> 8,
        xs & 0xff,
        xe >> 8,
        xe & 0xff,
    };

    // CASET (2Ah): Column Address Set
    st7789_cmd(0x2a, data, sizeof(data));
}

void st7789_raset(uint16_t ys, uint16_t ye)
{
    uint8_t data[] = {
        ys >> 8,
        ys & 0xff,
        ye >> 8,
        ye & 0xff,
    };

    // RASET (2Bh): Row Address Set
    st7789_cmd(0x2b, data, sizeof(data));
}

void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height)
{
    memcpy(&st7789_cfg, config, sizeof(st7789_cfg));
    st7789_width = width;
    st7789_height = height;

    // The original value requested 625 MHz and was clamped to half the
    // overclocked RP2040 system clock. That is far outside this panel's
    // reliable SPI range. Match the known-good Waveshare driver initially.
    spi_init(st7789_cfg.spi, ST7789_SPI_BAUDRATE);
    if (st7789_cfg.gpio_cs > -1) {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }

    gpio_set_function(st7789_cfg.gpio_din, GPIO_FUNC_SPI);
    gpio_set_function(st7789_cfg.gpio_clk, GPIO_FUNC_SPI);

    if (st7789_cfg.gpio_cs > -1) {
        gpio_init(st7789_cfg.gpio_cs);
    }
    gpio_init(st7789_cfg.gpio_dc);
    gpio_init(st7789_cfg.gpio_rst);
    gpio_init(st7789_cfg.gpio_bl);

    if (st7789_cfg.gpio_cs > -1) {
        gpio_set_dir(st7789_cfg.gpio_cs, GPIO_OUT);
    }
    gpio_set_dir(st7789_cfg.gpio_dc, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_rst, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_bl, GPIO_OUT);

    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 1);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    gpio_put(st7789_cfg.gpio_rst, 1);
    sleep_ms(100);
    
    // SWRESET (01h): Software Reset
    st7789_cmd(0x01, NULL, 0);
    sleep_ms(150);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(50);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 16bit/pixel
    st7789_cmd(0x3a, (uint8_t[]){ 0x05 }, 1);
    sleep_ms(10);

    // MADCTL (36h): Memory Data Access Control
    // - Page Address Order            = Top to Bottom
    // - Column Address Order          = Left to Right
    // - Page/Column Order             = Normal Mode
    // - Line Address Order            = LCD Refresh Top to Bottom
    // - RGB/BGR Order                 = RGB
    // - Display Data Latch Data Order = LCD Refresh Left to Right
    st7789_cmd(0x36, (uint8_t[]){ 0x70 }, 1);
   
    st7789_caset(0, width - 1);
    st7789_raset(0, height - 1);

    // INVON (21h): Display Inversion On
    st7789_cmd(0x21, NULL, 0);
    sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(0x13, NULL, 0);
    sleep_ms(10);

    // DISPON (29h): Display On
    st7789_cmd(0x29, NULL, 0);
    sleep_ms(10);

    gpio_put(st7789_cfg.gpio_bl, 1);
}

void st7789_ramwr()
{
    loop_delay();
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
    gpio_put(st7789_cfg.gpio_dc, 0);
    loop_delay();

    // RAMWR (2Ch): Memory Write
    uint8_t cmd = 0x2c;
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));

    loop_delay();
    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    loop_delay();
}

void st7789_write(const void* data, size_t len)
{
    if (!st7789_data_mode) {
        st7789_ramwr();

        if (st7789_cfg.gpio_cs > -1) {
            spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        } else {
            spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
        }

        st7789_data_mode = true;
    }

    spi_write16_blocking(st7789_cfg.spi, data, len / 2);
}

void st7789_put(uint16_t pixel)
{
    st7789_write(&pixel, sizeof(pixel));
}

void st7789_fill(uint16_t pixel)
{
    int num_pixels = st7789_width * st7789_height;

    st7789_set_cursor(0, 0);

    for (int i = 0; i < num_pixels; i++) {
        st7789_put(pixel);
    }
}

void st7789_set_cursor(uint16_t x, uint16_t y)
{
    st7789_set_window(x, y, st7789_width - x, st7789_height - y);
}

void st7789_set_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    st7789_caset(x, x + width - 1);
    st7789_raset(y, y + height - 1);
}

void st7789_vertical_scroll(uint16_t row)
{
    uint8_t data[] = {
        (row >> 8) & 0xff,
        row & 0x00ff
    };

    // VSCSAD (37h): Vertical Scroll Start Address of RAM 
    st7789_cmd(0x37, data, sizeof(data));
}

void st7789_partial_area(uint16_t start, uint16_t end)
{
    uint8_t data[] = {
        (start >> 8) & 0xff,
        start & 0x00ff,
        (end >> 8) & 0xff,
        end & 0x00ff,
    };

    // PTLAR (30h): Partial Area
    st7789_cmd(0x30, data, sizeof(data));
    // PTLON (12h): Partial Display Mode On
    st7789_cmd(0x12, NULL, 0);
}
