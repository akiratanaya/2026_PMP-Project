# Arduino Uno Makefile
MCU = atmega328p
F_CPU = 16000000UL
BAUD = 115200
PORT = /dev/ttyUSB0
PROGRAMMER = arduino
ROLE ?= master
AVRDUDE_FLAGS ?= -D -V -F

CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

CFLAGS = -DF_CPU=$(F_CPU) -mmcu=$(MCU) -Os -Ilibrary/src -DBOARD_UNO_R3
TARGET = program

ifeq ($(ROLE),master)
SOURCES = \
	library/src/arduino_board.c \
	library/src/arduino_gpio.c \
	library/src/arduino_adc.c \
	library/src/arduino_pwm.c \
	library/src/arduino_uart.c \
	library/src/arduino_time.c \
	library/src/arduino_i2c.c \
	library/src/inventory.c \
	library/src/serial_cmd.c \
	src/main.c
else ifeq ($(ROLE),slave)
SOURCES = \
	library/src/arduino_board.c \
	library/src/arduino_gpio.c \
	library/src/arduino_time.c \
	library/src/arduino_i2c.c \
	src/i2c_slave_main.c
else
$(error Unknown ROLE value: $(ROLE))
endif

all: $(TARGET).hex

$(TARGET).elf: $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

upload: $(TARGET).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U flash:w:$<:i

master:
	$(MAKE) clean
	$(MAKE) all ROLE=master

slave:
	$(MAKE) clean
	$(MAKE) all ROLE=slave

clean:
	rm -f $(TARGET).elf $(TARGET).hex host_csv

host_csv: host_csv.c
	gcc -O2 -Wall host_csv.c -o host_csv

.PHONY: all upload clean master slave host_csv
