#include "platform/network_credentials.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/platform.h"

#define CREDENTIAL_MAGIC 0x57494649u
#define CREDENTIAL_VERSION 1u
#define CREDENTIAL_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

typedef struct StoredCredentials
{
    uint32_t magic;
    uint32_t version;
    NetworkCredentials credentials;
    uint32_t checksum;
} StoredCredentials;

typedef struct FlashWrite
{
    uint8_t page[FLASH_PAGE_SIZE];
} FlashWrite;

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

static uint32_t record_checksum(const StoredCredentials *record)
{
    return checksum_bytes(record, offsetof(StoredCredentials, checksum));
}

static bool storage_does_not_overlap_firmware(void)
{
    const uintptr_t binary_end =
        (uintptr_t)&__flash_binary_end - (uintptr_t)XIP_BASE;
    return binary_end <= CREDENTIAL_FLASH_OFFSET;
}

static void write_page(void *parameter)
{
    const FlashWrite *write = parameter;
    flash_range_erase(CREDENTIAL_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(
        CREDENTIAL_FLASH_OFFSET,
        write->page,
        sizeof(write->page));
}

static bool write_record(const StoredCredentials *record)
{
    if (!storage_does_not_overlap_firmware())
    {
        return false;
    }

    FlashWrite write;
    memset(&write, 0xff, sizeof(write));
    if (record != NULL)
    {
        memcpy(write.page, record, sizeof(*record));
    }

    return flash_safe_execute(write_page, &write, 1000) == PICO_OK;
}

bool network_credentials_load(NetworkCredentials *credentials)
{
    if (credentials == NULL || !storage_does_not_overlap_firmware())
    {
        return false;
    }

    const StoredCredentials *record = (const StoredCredentials *)(
        XIP_BASE + CREDENTIAL_FLASH_OFFSET);
    if (record->magic != CREDENTIAL_MAGIC ||
        record->version != CREDENTIAL_VERSION ||
        record->checksum != record_checksum(record) ||
        record->credentials.ssid[0] == '\0' ||
        memchr(record->credentials.ssid, '\0', NETWORK_SSID_CAPACITY) == NULL ||
        memchr(
            record->credentials.password,
            '\0',
            NETWORK_PASSWORD_CAPACITY) == NULL)
    {
        return false;
    }

    *credentials = record->credentials;
    return true;
}

bool network_credentials_save(const NetworkCredentials *credentials)
{
    if (credentials == NULL || credentials->ssid[0] == '\0')
    {
        return false;
    }

    StoredCredentials record = {
        .magic = CREDENTIAL_MAGIC,
        .version = CREDENTIAL_VERSION,
        .credentials = *credentials,
    };
    record.credentials.ssid[NETWORK_SSID_CAPACITY - 1] = '\0';
    record.credentials.password[NETWORK_PASSWORD_CAPACITY - 1] = '\0';
    record.checksum = record_checksum(&record);
    return write_record(&record);
}

bool network_credentials_clear(void)
{
    return write_record(NULL);
}
