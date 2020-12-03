BUILD_DIR = ./build
Ttext = 0xc0001000
AS = nasm
CC = i386-elf-gcc
LD = i386-elf-ld
LIB = -I lib/ -I kernel/ -I device/ -I lib/kernel/ 
ASFLAGS = -f elf 
CFLAGS = $(LIB) -c -fno-builtin 
LDFLAGS = -Ttext $(Ttext) -e main

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
	   $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o \
	   $(BUILD_DIR)/assert.o $(BUILD_DIR)/string.o $(BUILD_DIR)/memory.o \
	   $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o

$(BUILD_DIR)/main.o: kernel/main.c lib/kernel/print.h lib/kernel/stdint.h kernel/init.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h lib/kernel/print.h\
					lib/kernel/stdint.h kernel/interrupt.h device/timer.h lib/kernel/memory.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h lib/kernel/stdint.h\
					kernel/global.h lib/kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/timer.o: device/timer.c device/timer.h lib/kernel/stdint.h \
				lib/kernel/io.h lib/kernel/print.h

	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/assert.o: kernel/assert.c kernel/assert.h lib/kernel/print.h\
				lib/kernel/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h lib/kernel/print.h\
			lib/kernel/stdint.h kernel/interrupt.h kernel/assert.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/string.o: lib/kernel/string.c lib/kernel/string.h kernel/assert.h lib/kernel/print.h lib/kernel/stdint.h 
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/memory.o: lib/kernel/memory.c lib/kernel/memory.h lib/kernel/print.h\
			lib/kernel/stdint.h lib/kernel/bitmap.h kernel/assert.h
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.S lib/include/head.inc
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY: build clean init dd all 

build: $(BUILD_DIR)/kernel.bin

clean:
	rm -rf ./build/*.bin
	rm -rf ./build/*.lock
	rm -rf ./build/*.img
	rm -rf ./*img
	rm -rf ./build/*.o
	rm -rf ./*.bin
init:
	bximage -hd=10 -mode=create -sectsize=512 -q ./hd3M.img

dd:
	nasm -I ./boot/include -o mbr.bin ./boot/mbr.S
	nasm -I ./boot/include -o loader.bin ./boot/loader.S
	dd if=./mbr.bin of=./hd3M.img bs=512 count=1 conv=notrunc
	dd if=./loader.bin of=./hd3M.img bs=512 count=4 seek=2 conv=notrunc
	dd if=build/kernel.bin of=hd3M.img bs=512 count=200 seek=9 conv=notrunc

all: clean build init dd 