#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "platform/display.h"

static DisplayColor pixel_at(const uint8_t *bytes, uint16_t width, int x, int y)
{
    const size_t offset = ((size_t)y * width + (size_t)x) * 2u;
    return (DisplayColor)(((uint16_t)bytes[offset] << 8) | bytes[offset + 1]);
}

int main(void)
{
    uint8_t bytes[5 * 4 * 2];
    memset(bytes, 0, sizeof(bytes));
    DisplaySurface surface = {
        .pixels = (uint16_t *)bytes,
        .width = 5,
        .height = 4,
    };

    display_fill_rectangle(&surface, -1, 1, 4, 2, 0x1234);
    assert(pixel_at(bytes, 5, 0, 1) == 0x1234);
    assert(pixel_at(bytes, 5, 2, 2) == 0x1234);
    assert(pixel_at(bytes, 5, 3, 2) == 0x0000);

    const uint8_t sprite[] = {
        1, 0, 2,
        2, 1, 0,
    };
    const DisplayColor palette[] = {0x0000, 0xabcd, 0x55aa};
    display_blit_indexed(
        &surface, 1, 0, sprite, 3, 2, palette, 3, 0, false);
    assert(pixel_at(bytes, 5, 1, 0) == 0xabcd);
    assert(pixel_at(bytes, 5, 2, 0) == 0x0000);
    assert(pixel_at(bytes, 5, 3, 0) == 0x55aa);

    display_blit_indexed(
        &surface, -1, 3, sprite, 3, 2, palette, 3, 0, true);
    assert(pixel_at(bytes, 5, 0, 3) == 0x0000);
    assert(pixel_at(bytes, 5, 1, 3) == 0xabcd);

    memset(bytes, 0, sizeof(bytes));
    display_blit_indexed_scaled(
        &surface, 0, 0, sprite, 3, 2, palette, 3, 0, false, 2);
    assert(pixel_at(bytes, 5, 0, 0) == 0xabcd);
    assert(pixel_at(bytes, 5, 1, 1) == 0xabcd);
    assert(pixel_at(bytes, 5, 2, 0) == 0x0000);
    assert(pixel_at(bytes, 5, 4, 1) == 0x55aa);
    assert(pixel_at(bytes, 5, 0, 2) == 0x55aa);

    return 0;
}
