DEVICE     = attiny25
PROGRAMMER = stk500v1
BAUD       = 9600
FILENAME   = main
CC		   = avr-gcc	
CFLAGS     = -Wall -Wextra -Os -mmcu=$(DEVICE)

# RSTPIN is used to drive the LEDs in the game, and requires RSTDSBL fuse to be set.
# This breaks ISP programming so we do it only for the final release. Normally, the 
# game will skit the LEDs tied to reset pin.
ifeq ($(TYPE),Release)
	PROG_ARGS = -U hfuse:w:0x5f:m
else
	CFLAGS += -DSKIP_RST_PIN
endif

all: build upload

build:
	$(CC) $(CFLAGS) -c $(FILENAME).c -o $(FILENAME).o
	$(CC) $(CFLAGS) -o $(FILENAME).elf $(FILENAME).o
	avr-objcopy -j .text -j .data -O ihex $(FILENAME).elf $(FILENAME).hex
	avr-size --format=avr --mcu=$(DEVICE) $(FILENAME).elf

upload:
	avrdude -v -p $(DEVICE) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U flash:w:$(FILENAME).hex:i $(PROG_ARGS)

clean:
	rm main.o
	rm main.elf
	rm main.hex
