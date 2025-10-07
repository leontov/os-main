PREFIX ?= i686-elf
CC := $(PREFIX)-gcc
LD := $(PREFIX)-ld
AS := nasm
GRUBMKRESCUE ?= grub-mkrescue
OBJCOPY := $(PREFIX)-objcopy

CFLAGS := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Wpedantic -m32
LDFLAGS := -nostdlib

BUILD_DIR := build
ISO_DIR := iso
TARGET := $(BUILD_DIR)/kolibri.elf
ISO := $(BUILD_DIR)/kolibri.iso

OBJS := $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o

all: $(ISO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: boot/boot.asm | $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) kernel/link.ld
	$(LD) -m elf_i386 -T kernel/link.ld -o $@ $(OBJS)

$(ISO): $(TARGET)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(TARGET) $(ISO_DIR)/boot/kernel.elf
	echo 'set timeout=0' > $(ISO_DIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "Kolibri OS" { multiboot /boot/kernel.elf; boot }' >> $(ISO_DIR)/boot/grub/grub.cfg
	$(GRUBMKRESCUE) -o $@ $(ISO_DIR)

run: $(TARGET)
	qemu-system-i386 -kernel $(TARGET) -serial stdio -display none

iso-run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -serial stdio

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR)

.PHONY: all clean run iso-run
