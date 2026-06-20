#ifndef ARDUINO_I2C_H
#define ARDUINO_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_OK 0
#define I2C_ERROR 1
#define I2C_NACK 2
#define I2C_TIMEOUT 3

void i2c_master_init(uint32_t scl_hz);
uint8_t i2c_master_start(uint8_t address_rw);
uint8_t i2c_master_write(uint8_t data);
uint8_t i2c_master_read_ack(uint8_t *data);
uint8_t i2c_master_read_nack(uint8_t *data);
void i2c_master_stop(void);
uint8_t i2c_master_mem_write(uint8_t slave_address, uint16_t offset, const uint8_t *data, uint8_t length);
uint8_t i2c_master_mem_read(uint8_t slave_address, uint16_t offset, uint8_t *data, uint8_t length);

void i2c_slave_init(uint8_t slave_address, uint8_t *memory, uint16_t memory_size);
uint8_t i2c_slave_take_dirty_flag(void);

#ifdef __cplusplus
}
#endif

#endif