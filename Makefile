DEVICE     = atmega328p
CLOCK      = 16000000
PROGRAMMER = -c arduino -b 115200 -P COM5
FUSES      = -U hfuse:w:0xde:m -U lfuse:w:0xff:m -U efuse:w:0x05:m

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)

CC      = avr-gcc
TARGET  = main

# --- Auto drivers/<name>/<name>.c and include paths ---
DRV_DIRS  := $(wildcard drivers/*)
INCLUDES  := -I. -Idrivers $(addprefix -I,$(DRV_DIRS))

# Auto-include all C sources in root + all driver subfolders
SRC     := $(sort $(wildcard *.c) $(wildcard drivers/*/*.c))
OBJECTS := $(SRC:.c=.o)

CFLAGS  = -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -std=c99 $(INCLUDES)

# (Optional but useful for smaller printf)
LDFLAGS = -Wl,-u,vfprintf -lprintf_min

.PHONY: all flash fuse install clean disasm cpp

all: $(TARGET).hex

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.S.o:
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(CC) $(CFLAGS) -S $< -o $@

flash: all
	$(AVRDUDE) -U flash:w:$(TARGET).hex:i

fuse:
	$(AVRDUDE) $(FUSES)

install: flash fuse

clean:
	rm -f $(TARGET).hex $(TARGET).elf $(OBJECTS)

$(TARGET).elf: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET).elf $(OBJECTS) $(LDFLAGS)

$(TARGET).hex: $(TARGET).elf
	rm -f $(TARGET).hex
	avr-objcopy -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex
	avr-size --format=avr --mcu=$(DEVICE) $(TARGET).elf

disasm: $(TARGET).elf
	avr-objdump -d $(TARGET).elf

cpp:
	$(CC) $(CFLAGS) -E $(TARGET).c
