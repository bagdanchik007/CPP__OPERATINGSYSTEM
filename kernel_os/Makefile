# Voraussetzungen (Ubuntu/Debian):
#   sudo apt install nasm xorriso grub-pc-bin grub-common gcc-x86-64-linux-gnu
#
# Wir cross-compilen mit dem normalen g++, aber mit Freestanding-Flags -
# für ein "richtiges" Setup später einen x86_64-elf-gcc Cross-Compiler
# besorgen (siehe OSDev Wiki: "GCC Cross-Compiler").

CXX      = g++
AS       = g++
LD       = ld

CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -mno-red-zone \
           -mno-mmx -mno-sse -mno-sse2 -m64 -Wall -Wextra \
           -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
           -Iinclude -c

ASFLAGS  = -c

LDFLAGS  = -T src/linker.ld -nostdlib -static -no-pie

SRC_DIR  = src
BUILD_DIR = build
ISO_DIR  = isodir

CPP_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
ASM_SOURCES = $(SRC_DIR)/boot.S $(SRC_DIR)/context_switch.S \
              $(SRC_DIR)/gdt_flush.S $(SRC_DIR)/idt_load.S $(SRC_DIR)/isr_stubs.S

ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.S,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

OBJECTS = $(ASM_OBJECTS) \
          $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CPP_SOURCES))

KERNEL_BIN = $(BUILD_DIR)/kernel.bin
ISO_FILE   = myos.iso

.PHONY: all clean run iso

all: $(ISO_FILE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(KERNEL_BIN): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

iso: $(KERNEL_BIN)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

$(ISO_FILE): iso

run: $(ISO_FILE)
	qemu-system-x86_64 -cdrom $(ISO_FILE) -m 256M

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_FILE)
