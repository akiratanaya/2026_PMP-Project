#include "../library/src/arduino.h"
#include <avr/eeprom.h>
#include <stddef.h>

#define I2C_SLAVE_ADDRESS 0x08U
#define I2C_SLAVE_MEMORY_SIZE 640U
#define SLAVE_NV_MAGIC 0x534C5631UL

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint16_t payload_size;
    uint16_t crc;
} slave_nv_header_t;

static uint8_t slave_memory[I2C_SLAVE_MEMORY_SIZE];
static slave_nv_header_t EEMEM ee_nv_header;
static uint8_t EEMEM ee_nv_payload[I2C_SLAVE_MEMORY_SIZE];

static uint16_t checksum16(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0U;
    uint16_t i;

    for (i = 0U; i < len; ++i)
    {
        sum += p[i];
    }

    return (uint16_t)(sum & 0xFFFFU);
}

static void nv_load(void)
{
    slave_nv_header_t hdr;
    uint16_t crc_expected;

    eeprom_read_block((void *)&hdr, (const void *)&ee_nv_header, sizeof(hdr));
    if ((hdr.magic != SLAVE_NV_MAGIC) || (hdr.payload_size != I2C_SLAVE_MEMORY_SIZE))
    {
        for (uint16_t i = 0U; i < I2C_SLAVE_MEMORY_SIZE; ++i)
        {
            slave_memory[i] = 0U;
        }
        return;
    }

    eeprom_read_block((void *)slave_memory, (const void *)ee_nv_payload, I2C_SLAVE_MEMORY_SIZE);
    crc_expected = checksum16(slave_memory, I2C_SLAVE_MEMORY_SIZE);
    if (crc_expected != hdr.crc)
    {
        for (uint16_t i = 0U; i < I2C_SLAVE_MEMORY_SIZE; ++i)
        {
            slave_memory[i] = 0U;
        }
    }
}

static void nv_save(void)
{
    slave_nv_header_t hdr;

    hdr.magic = SLAVE_NV_MAGIC;
    hdr.payload_size = I2C_SLAVE_MEMORY_SIZE;
    hdr.crc = checksum16(slave_memory, I2C_SLAVE_MEMORY_SIZE);

    eeprom_update_block((const void *)slave_memory, (void *)ee_nv_payload, I2C_SLAVE_MEMORY_SIZE);
    eeprom_update_block((const void *)&hdr, (void *)&ee_nv_header, sizeof(hdr));
}

int main(void)
{
    board_init();
    nv_load();
    pin_mode(13U, BOARD_OUTPUT);
    digital_write(13U, BOARD_LOW);
    i2c_slave_init(I2C_SLAVE_ADDRESS, slave_memory, I2C_SLAVE_MEMORY_SIZE);

    uint8_t save_pending = 0U;
    uint8_t idle_ticks = 0U;

    while (1)
    {
        if (i2c_slave_take_dirty_flag() != 0U)
        {
            save_pending = 1U;
            idle_ticks = 0U;
            digital_write(13U, BOARD_HIGH);
        }

        if (save_pending != 0U)
        {
            idle_ticks++;
            if (idle_ticks > 50U) /* 50 * 10ms = 500ms idle timeout */
            {
                nv_save();
                save_pending = 0U;
                digital_write(13U, BOARD_LOW);
            }
        }

        delay_ms(10U);
    }

    return 0;
}