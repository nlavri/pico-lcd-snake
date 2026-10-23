#include "screen.h"
#include "hardware/gpio.h"

#include "screens/st7789_240_135.h"

void I_initScreen(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    st7789_240_135_initScreen();
}

void I_handleFrameStart(uint8_t frame) {
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    st7789_240_135_handleFrameStart(frame);
}

void I_handleScanline(uint16_t *line, int scanline) {
    st7789_240_135_handleScanline(line, scanline);
}

void I_handleFrameEnd(uint8_t frame) {
    (void)frame;
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

void I_showDiagnosticColor(uint16_t color) {
    st7789_fill(color);
}
