all: clean useless_box.hex send

useless_box.hex: useless_box.elf
	avr-objcopy  -j .text -j .data -O ihex $^ $@
	avr-size useless_box.elf

useless_box.elf: useless_box.c ssd1306.c I2C.c
	avr-g++ -mmcu=atmega324p -DF_CPU=16000000 -Os -Wall -o $@ $^

send:
	sudo ./bootloader/commandline/bootloadHID useless_box.hex

clean:
	rm -rf useless_box.elf useless_box.hex
