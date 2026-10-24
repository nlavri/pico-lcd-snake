#ifndef PICO_LCD_PLATFORM_RP2040_SPI_DMA_H
#define PICO_LCD_PLATFORM_RP2040_SPI_DMA_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"

// Starts sending a contiguous byte buffer through an SPI TX FIFO using a
// dynamically claimed DMA channel. Returns true while a DMA transfer is in
// progress. If no channel is available, completes through blocking SPI and
// returns false.
bool platform_spi_dma_start(
    spi_inst_t *spi,
    const uint8_t *data,
    size_t length);

// Returns true after DMA and the SPI shifter have both finished. The source
// buffer must not be changed before this reports completion.
bool platform_spi_dma_is_complete(spi_inst_t *spi);

// Synchronous wrapper used during display initialization.
void platform_spi_dma_write_blocking(
    spi_inst_t *spi,
    const uint8_t *data,
    size_t length);

#endif
