#include "touch.h"

#include "common/logger.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

enum
{
    TOUCH_SDA_PIN = 8,
    TOUCH_SCL_PIN = 9,
    TOUCH_RESET_PIN = 10,
    TOUCH_INTERRUPT_PIN = 11,
    GT911_ADDRESS_PRIMARY = 0x5d,
    GT911_ADDRESS_SECONDARY = 0x14,
    GT911_CONFIG_START = 0x8047,
    GT911_PRODUCT_ID = 0x8140,
    GT911_STATUS = 0x814e,
    GT911_FIRST_POINT = 0x814f,
};

static uint8_t touch_address;
static bool touch_available;
static uint8_t detected_product_id[4];
static uint8_t detected_config[5];
static bool touch_pressed;
static uint16_t last_x;
static uint16_t last_y;

static uint16_t clamp_coordinate(uint16_t value, uint16_t maximum)
{
    return value < maximum ? value : (uint16_t)(maximum - 1);
}

static bool gt911_read(uint16_t reg, uint8_t *data, size_t length)
{
    const uint8_t address[] = {
        (uint8_t)(reg >> 8),
        (uint8_t)reg,
    };
    return i2c_write_blocking(
               i2c0, touch_address, address, sizeof(address), true) ==
            (int)sizeof(address) &&
        i2c_read_blocking(i2c0, touch_address, data, length, false) ==
            (int)length;
}

static bool gt911_write_byte(uint16_t reg, uint8_t value)
{
    const uint8_t data[] = {
        (uint8_t)(reg >> 8),
        (uint8_t)reg,
        value,
    };
    return i2c_write_blocking(
               i2c0, touch_address, data, sizeof(data), false) ==
        (int)sizeof(data);
}

static bool address_responds(uint8_t address)
{
    touch_address = address;
    uint8_t product_id[4];
    return gt911_read(GT911_PRODUCT_ID, product_id, sizeof(product_id));
}

bool ep0172_touch_init(void)
{
    /*
     * GT911 INT is bidirectional during reset. Holding it low selects 0x5d
     * and, on EP-0172 revisions seen in the field, also prevents the
     * controller from remaining asleep after reset.
     */
    gpio_init(TOUCH_RESET_PIN);
    gpio_set_dir(TOUCH_RESET_PIN, GPIO_OUT);
    gpio_put(TOUCH_RESET_PIN, 0);
    gpio_init(TOUCH_INTERRUPT_PIN);
    gpio_set_dir(TOUCH_INTERRUPT_PIN, GPIO_OUT);
    gpio_put(TOUCH_INTERRUPT_PIN, 0);
    sleep_ms(20);
    gpio_put(TOUCH_RESET_PIN, 1);
    sleep_ms(5);
    gpio_put(TOUCH_INTERRUPT_PIN, 0);
    sleep_ms(50);
    gpio_set_dir(TOUCH_INTERRUPT_PIN, GPIO_IN);
    sleep_ms(200);

    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(TOUCH_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(TOUCH_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(TOUCH_SDA_PIN);
    gpio_pull_up(TOUCH_SCL_PIN);

    if (!address_responds(GT911_ADDRESS_PRIMARY) &&
        !address_responds(GT911_ADDRESS_SECONDARY))
    {
        LOG_WARNING("touch", "GT911 did not respond at 0x5d or 0x14");
        return false;
    }

    if (!gt911_read(
            GT911_PRODUCT_ID,
            detected_product_id,
            sizeof(detected_product_id)) ||
        !gt911_read(
            GT911_CONFIG_START,
            detected_config,
            sizeof(detected_config)))
    {
        LOG_WARNING("touch", "GT911 identification read failed");
        return false;
    }

    const uint16_t configured_x =
        (uint16_t)(detected_config[1] |
            ((uint16_t)detected_config[2] << 8));
    const uint16_t configured_y =
        (uint16_t)(detected_config[3] |
            ((uint16_t)detected_config[4] << 8));
    LOG_INFO(
        "touch",
        "GT911 addr=0x%02x id=%02x%02x%02x%02x config=%u resolution=%ux%u",
        touch_address,
        detected_product_id[0],
        detected_product_id[1],
        detected_product_id[2],
        detected_product_id[3],
        detected_config[0],
        configured_x,
        configured_y);
    if (detected_config[0] == 0 || configured_x == 0 || configured_y == 0)
    {
        LOG_WARNING(
            "touch",
            "GT911 configuration is blank; leaving controller flash unchanged");
    }

    touch_available = true;
    return true;
}

bool ep0172_touch_read(uint16_t *x, uint16_t *y)
{
    if (!touch_available)
    {
        return false;
    }

    uint8_t status;
    if (!gt911_read(GT911_STATUS, &status, 1))
    {
        return false;
    }

    const uint8_t point_count = status & 0x0fu;
    if ((status & 0x80u) == 0)
    {
        *x = last_x;
        *y = last_y;
        return touch_pressed;
    }
    if (point_count == 0 || point_count > 5)
    {
        gt911_write_byte(GT911_STATUS, 0);
        touch_pressed = false;
        return false;
    }

    uint8_t point[8];
    if (!gt911_read(GT911_FIRST_POINT, point, sizeof(point)))
    {
        gt911_write_byte(GT911_STATUS, 0);
        return false;
    }
    gt911_write_byte(GT911_STATUS, 0);

    const uint16_t raw_x =
        (uint16_t)(point[1] | ((uint16_t)point[2] << 8));
    const uint16_t raw_y =
        (uint16_t)(point[3] | ((uint16_t)point[4] << 8));
    /* The panel reports portrait coordinates while the LCD is landscape. */
    last_x = clamp_coordinate(raw_y, 480);
    last_y = clamp_coordinate(
        raw_x < 320 ? (uint16_t)(319 - raw_x) : 0,
        320);
    *x = last_x;
    *y = last_y;
    touch_pressed = true;

    return true;
}
