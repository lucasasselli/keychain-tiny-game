DEVICE     = attiny25
PROGRAMMER = stk500v1
BAUD       = 9600
FILENAME   = main
COMPILE    = avr-gcc -Wall -Os -mmcu=$(DEVICE)

all: build upload

build:
	$(COMPILE) -c $(FILENAME).c -o $(FILENAME).o
	$(COMPILE) -o $(FILENAME).elf $(FILENAME).o
	avr-objcopy -j .text -j .data -O ihex $(FILENAME).elf $(FILENAME).hex
	avr-size --format=avr --mcu=$(DEVICE) $(FILENAME).elf

upload:
	avrdude -v -p $(DEVICE) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U flash:w:$(FILENAME).hex:i

clean:
	rm main.o
	rm main.elf
	rm main.hex
