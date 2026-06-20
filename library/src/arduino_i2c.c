#include "arduino_i2c.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>
static uint8_t *i2c_slave_memory = 0;
static uint16_t i2c_slave_memory_size = 0;
static uint16_t i2c_slave_index = 0;
static uint8_t i2c_slave_offset_bytes = 0;
static uint8_t i2c_slave_index_valid = 0;
static volatile uint8_t i2c_slave_dirty = 0;
static uint8_t i2c_wait_complete(void)
{
    uint16_t guard = 60000;
    while ((TWCR & _BV(TWINT)) == 0)
    {
        if (guard == 0)
        {
            return I2C_TIMEOUT; 
        }
        guard--;
    }
    return I2C_OK;
}
static uint8_t i2c_status_to_error(uint8_t status)
{
    if (status == TW_MT_SLA_ACK || status == TW_MT_DATA_ACK || status == TW_MR_SLA_ACK || status == TW_MR_DATA_ACK || status == TW_MR_DATA_NACK)
    {
        return I2C_OK; 
    }
    if (status == TW_MT_SLA_NACK || status == TW_MT_DATA_NACK || status == TW_MR_SLA_NACK)
    {
        return I2C_NACK; 
    }
    return I2C_ERROR;
}
void i2c_master_init(uint32_t scl_hz)
{
    uint32_t twbr_value;
    if (scl_hz == 0)
    {
        scl_hz = 100000UL; 
    }
    TWSR = 0;
    DDRC &= (uint8_t)~(_BV(PC4) | _BV(PC5));
    PORTC |= _BV(PC4) | _BV(PC5);
    twbr_value = ((F_CPU / scl_hz) - 16UL) / 2UL;
    if (twbr_value > 255UL)
    {
        twbr_value = 255UL;
    }
    TWBR = (uint8_t)twbr_value;
    TWCR = _BV(TWEN) | _BV(TWINT);
}
uint8_t i2c_master_start(uint8_t address_rw)
{
    uint8_t result;
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    result = i2c_wait_complete();
    if (result != I2C_OK)
    {
        return result;
    }
    if ((TW_STATUS != TW_START) && (TW_STATUS != TW_REP_START))
    {
        return I2C_ERROR;
    }
    TWDR = address_rw;
    TWCR = _BV(TWINT) | _BV(TWEN);
    result = i2c_wait_complete();
    if (result != I2C_OK)
    {
        return result;
    }
    return i2c_status_to_error(TW_STATUS);
}
uint8_t i2c_master_write(uint8_t data)
{
    uint8_t result;
    TWDR = data;
    TWCR = _BV(TWINT) | _BV(TWEN);
    result = i2c_wait_complete();
    if (result != I2C_OK)
    {
        return result;
    }
    return i2c_status_to_error(TW_STATUS);
}
uint8_t i2c_master_read_ack(uint8_t *data)
{
    uint8_t result;
    if (data == 0)
    {
        return I2C_ERROR;
    }
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
    result = i2c_wait_complete();
    if (result != I2C_OK)
    {
        return result;
    }
    if (TW_STATUS != TW_MR_DATA_ACK)
    {
        return i2c_status_to_error(TW_STATUS);
    }
    *data = TWDR;
    return I2C_OK;
}
uint8_t i2c_master_read_nack(uint8_t *data)
{
    uint8_t result;
    if (data == 0)
    {
        return I2C_ERROR;
    }
    TWCR = _BV(TWINT) | _BV(TWEN);
    result = i2c_wait_complete();
    if (result != I2C_OK)
    {
        return result;
    }
    if (TW_STATUS != TW_MR_DATA_NACK)
    {
        return i2c_status_to_error(TW_STATUS);
    }
    *data = TWDR;
    return I2C_OK;
}
void i2c_master_stop(void)
{
    uint16_t guard = 60000;
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
    while ((TWCR & _BV(TWSTO)) != 0)
    {
        if (guard == 0)
        {
            break;
        }
        guard--;
    }
}
uint8_t i2c_master_mem_write(uint8_t slave_address, uint16_t offset, const uint8_t *data, uint8_t length)
{
    uint8_t status;
    uint8_t i;
    if ((data == 0) && (length > 0))
    {
        return I2C_ERROR;
    }
    status = i2c_master_start((uint8_t)((slave_address << 1) | 0));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    status = i2c_master_write((uint8_t)(offset >> 8));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    status = i2c_master_write((uint8_t)(offset & 0x00FF));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    for (i = 0; i < length; ++i)
    {
        status = i2c_master_write(data[i]);
        if (status != I2C_OK)
        {
            i2c_master_stop();
            return status;
        }
    }
    i2c_master_stop();
    return I2C_OK;
}
uint8_t i2c_master_mem_read(uint8_t slave_address, uint16_t offset, uint8_t *data, uint8_t length)
{
    uint8_t status;
    uint8_t i;
    if ((data == 0) || (length == 0))
    {
        return I2C_ERROR;
    }
    status = i2c_master_start((uint8_t)((slave_address << 1) | 0));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    status = i2c_master_write((uint8_t)(offset >> 8));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    status = i2c_master_write((uint8_t)(offset & 0x00FF));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    status = i2c_master_start((uint8_t)((slave_address << 1) | 1));
    if (status != I2C_OK)
    {
        i2c_master_stop();
        return status;
    }
    for (i = 0; i < (uint8_t)(length - 1); ++i)
    {
        status = i2c_master_read_ack(&data[i]);
        if (status != I2C_OK)
        {
            i2c_master_stop();
            return status;
        }
    }
    status = i2c_master_read_nack(&data[length - 1]);
    i2c_master_stop();
    return status;
}
void i2c_slave_init(uint8_t slave_address, uint8_t *memory, uint16_t memory_size)
{
    i2c_slave_memory = memory;
    i2c_slave_memory_size = memory_size;
    i2c_slave_index = 0;
    i2c_slave_offset_bytes = 0;
    i2c_slave_index_valid = 0;
    i2c_slave_dirty = 0;
    DDRC &= (uint8_t)~(_BV(PC4) | _BV(PC5));
    PORTC |= _BV(PC4) | _BV(PC5);
    TWAR = (uint8_t)(slave_address << 1);
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
    sei();
}
uint8_t i2c_slave_take_dirty_flag(void)
{
    uint8_t dirty;
    cli();
    dirty = i2c_slave_dirty;
    i2c_slave_dirty = 0;
    sei();
    return dirty;
}
ISR(TWI_vect)
{
    switch (TW_STATUS)
    {
        case TW_SR_SLA_ACK:
        case TW_SR_ARB_LOST_SLA_ACK:
        case TW_SR_GCALL_ACK:
        case TW_SR_ARB_LOST_GCALL_ACK:
            i2c_slave_offset_bytes = 0;
            i2c_slave_index_valid = 0;
            break;
        case TW_SR_DATA_ACK:
        case TW_SR_GCALL_DATA_ACK:
            if (i2c_slave_offset_bytes == 0)
            {
                i2c_slave_index = (uint16_t)TWDR << 8;
                i2c_slave_offset_bytes = 1;
            }
            else if (i2c_slave_offset_bytes == 1)
            {
                i2c_slave_index |= (uint16_t)TWDR;
                if (i2c_slave_index >= i2c_slave_memory_size)
                {
                    i2c_slave_index = 0;
                }
                i2c_slave_offset_bytes = 2;
                i2c_slave_index_valid = 1;
            }
            else if ((i2c_slave_memory != 0) && (i2c_slave_index < i2c_slave_memory_size))
            {
                i2c_slave_memory[i2c_slave_index] = TWDR;
                i2c_slave_dirty = 1;
                if (i2c_slave_index + 1 < i2c_slave_memory_size)
                {
                    i2c_slave_index++;
                }
            }
            break;
        case TW_ST_SLA_ACK:
        case TW_ST_ARB_LOST_SLA_ACK:
            if ((i2c_slave_memory != 0) && (i2c_slave_index_valid != 0) && (i2c_slave_index < i2c_slave_memory_size))
            {
                TWDR = i2c_slave_memory[i2c_slave_index];
                if (i2c_slave_index + 1 < i2c_slave_memory_size)
                {
                    i2c_slave_index++;
                }
            }
            else
            {
                TWDR = 0xFF;
            }
            break;
        case TW_ST_DATA_ACK:
            if ((i2c_slave_memory != 0) && (i2c_slave_index_valid != 0) && (i2c_slave_index < i2c_slave_memory_size))
            {
                TWDR = i2c_slave_memory[i2c_slave_index];
                if (i2c_slave_index + 1 < i2c_slave_memory_size)
                {
                    i2c_slave_index++;
                }
            }
            else
            {
                TWDR = 0xFF;
            }
            break;
        case TW_SR_STOP:
        case TW_SR_DATA_NACK:
        case TW_SR_GCALL_DATA_NACK:
        case TW_ST_DATA_NACK:
        default:
            break;
    }
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
}
