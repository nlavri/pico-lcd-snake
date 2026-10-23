#include "platform/preferences.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/platform.h"

#define PREFERENCES_MAGIC 0x50524546u
#define PREFERENCES_VERSION 2u
#define PREFERENCES_DEFAULT_BRIGHTNESS 50u
#define PREFERENCES_FLASH_OFFSET \
    (PICO_FLASH_SIZE_BYTES - 2u * FLASH_SECTOR_SIZE)

typedef struct StoredPreferences
{
    uint32_t magic;
    uint32_t version;
    uint8_t accent_index;
    uint8_t brightness_percent;
    uint8_t reserved[2];
    uint32_t checksum;
} StoredPreferences;

typedef struct StoredPreferencesV1
{
    uint32_t magic;
    uint32_t version;
    uint8_t accent_index;
    uint8_t reserved[3];
    uint32_t checksum;
} StoredPreferencesV1;

typedef struct PreferencesFlashWrite
{
    uint8_t page[FLASH_PAGE_SIZE];
} PreferencesFlashWrite;

extern uint8_t __flash_binary_end;

static uint32_t checksum_bytes(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t checksum = 2166136261u;
    for (size_t i = 0; i < size; i++)
    {
        checksum ^= bytes[i];
        checksum *= 16777619u;
    }
    return checksum;
}

static uint32_t record_checksum(const StoredPreferences *record)
{
    return checksum_bytes(record, offsetof(StoredPreferences, checksum));
}

static bool storage_does_not_overlap_firmware(void)
{
    const uintptr_t binary_end =
        (uintptr_t)&__flash_binary_end - (uintptr_t)XIP_BASE;
    return binary_end <= PREFERENCES_FLASH_OFFSET;
}

static void write_page(void *parameter)
{
    const PreferencesFlashWrite *write = parameter;
    flash_range_erase(PREFERENCES_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(
        PREFERENCES_FLASH_OFFSET,
        write->page,
        sizeof(write->page));
}

bool platform_preferences_load(PlatformPreferences *preferences)
{
    if (preferences == NULL || !storage_does_not_overlap_firmware())
    {
        return false;
    }

    const StoredPreferences *record = (const StoredPreferences *)(
        XIP_BASE + PREFERENCES_FLASH_OFFSET);
    if (record->magic != PREFERENCES_MAGIC)
    {
        return false;
    }

    if (record->version == PREFERENCES_VERSION &&
        record->checksum == record_checksum(record))
    {
        *preferences = (PlatformPreferences){
            .accent_index = record->accent_index,
            .brightness_percent = record->brightness_percent,
        };
        return true;
    }

    const StoredPreferencesV1 *legacy =
        (const StoredPreferencesV1 *)record;
    if (legacy->version == 1u &&
        legacy->checksum == checksum_bytes(
            legacy,
            offsetof(StoredPreferencesV1, checksum)))
    {
        *preferences = (PlatformPreferences){
            .accent_index = legacy->accent_index,
            .brightness_percent = PREFERENCES_DEFAULT_BRIGHTNESS,
        };
        return true;
    }
    return false;
}

bool platform_preferences_save(const PlatformPreferences *preferences)
{
    if (preferences == NULL || !storage_does_not_overlap_firmware())
    {
        return false;
    }

    StoredPreferences record = {
        .magic = PREFERENCES_MAGIC,
        .version = PREFERENCES_VERSION,
        .accent_index = preferences->accent_index,
        .brightness_percent = preferences->brightness_percent,
    };
    record.checksum = record_checksum(&record);

    PreferencesFlashWrite write;
    memset(&write, 0xff, sizeof(write));
    memcpy(write.page, &record, sizeof(record));
    return flash_safe_execute(write_page, &write, 1000) == PICO_OK;
}
