#include "spi_dma.h"

#include <limits.h>

#include "common/logger.h"
#include "hardware/dma.h"
#include "hardware/structs/spi.h"
#include "pico/stdlib.h"

static int dma_channel = -1;
static bool transfer_reported;
static bool transfer_active;
static uint32_t completed_transfer_count;
static uint32_t transfer_length;
static uint32_t transfer_started_us;

static bool ensure_dma_channel(void)
{
    if (dma_channel >= 0)
    {
        return true;
    }

    dma_channel = dma_claim_unused_channel(false);
    return dma_channel >= 0;
}

bool platform_spi_dma_start(
    spi_inst_t *spi,
    const uint8_t *data,
    size_t length)
{
    if (spi == NULL || data == NULL || length == 0 || transfer_active)
    {
        return false;
    }

    if (!ensure_dma_channel() || length > UINT32_MAX)
    {
        if (!transfer_reported)
        {
            LOG_WARNING(
                "display",
                "DMA unavailable; using blocking SPI");
            transfer_reported = true;
        }
        spi_write_blocking(spi, data, length);
        return false;
    }

    dma_channel_config config = dma_channel_get_default_config(
        (uint)dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, spi_get_dreq(spi, true));

    transfer_length = (uint32_t)length;
    transfer_started_us = time_us_32();
    transfer_active = true;
    dma_channel_configure(
        (uint)dma_channel,
        &config,
        &spi_get_hw(spi)->dr,
        data,
        (uint32_t)length,
        true);
    return true;
}

bool platform_spi_dma_is_complete(spi_inst_t *spi)
{
    if (!transfer_active)
    {
        return true;
    }

    if (dma_channel_is_busy((uint)dma_channel))
    {
        return false;
    }

    // DMA completion means that the final byte entered the SPI FIFO, not that
    // it has reached the panel. Keep chip-select asserted until the shifter is
    // empty.
    if (spi_is_busy(spi))
    {
        return false;
    }

    transfer_active = false;
    completed_transfer_count++;
    // Several startup redraws can happen before USB CDC reconnects after a
    // flash. Delay the one-shot report until user interaction makes it
    // observable without logging continuously during normal rendering.
    if (!transfer_reported && completed_transfer_count >= 8u)
    {
        LOG_INFO(
            "display",
            "DMA channel %d sent %lu bytes in %lu us",
            dma_channel,
            (unsigned long)transfer_length,
            (unsigned long)(time_us_32() - transfer_started_us));
        transfer_reported = true;
    }
    return true;
}

void platform_spi_dma_write_blocking(
    spi_inst_t *spi,
    const uint8_t *data,
    size_t length)
{
    if (!platform_spi_dma_start(spi, data, length))
    {
        return;
    }

    while (!platform_spi_dma_is_complete(spi))
    {
        tight_loop_contents();
    }
}
