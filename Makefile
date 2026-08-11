# ==================john_kernel=================
JLOS := johnkernel
ARCH := x86

all: $(JLOS).iso

GCPPARAMS = -m32 -I. -nostdlib -fno-builtin -fno-exceptions \
            -fno-leading-underscore -std=gnu11 \
            -ffreestanding -fno-stack-protector \
			-fno-pic -fno-pie \
            -Wall -Wextra -Wno-address-of-packed-member \
            -O2 -g
ASPARAMS = --32 -I.
LDPARAMS = -melf_i386

SRC_DIRS := kernel arch/$(ARCH) drivers net filesystem tools hal dsa
OBJ_DIR := obj

C_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.c")
AS_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.s")
C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS))
AS_OBJS := $(patsubst %.s,$(OBJ_DIR)/%.o,$(AS_SRCS))
OBJS := $(C_OBJS) $(AS_OBJS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@gcc $(GCPPARAMS) -o $@ -c $<

$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(@D)
	@as $(ASPARAMS) -o $@ $<

$(JLOS).bin: arch/$(ARCH)/linker.ld  $(OBJS)
	@ld $(LDPARAMS) -T $< -o $@  $(OBJS)

install: $(JLOS).bin
	@sudo cp $< /boot/$(JLOS).bin

$(JLOS).iso: $(JLOS).bin
	@mkdir -p iso/boot/grub
	@cp $< iso/boot/$(JLOS).bin
	@echo 'set timeout=0' >> iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo '' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "johnbeauty operating system" {' >> iso/boot/grub/grub.cfg
	@echo ' multiboot /boot/$(JLOS).bin' >> iso/boot/grub/grub.cfg
	@echo ' boot' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	@grub-mkrescue --output=$@ iso
	@rm -rf iso

clean:
	@rm -rf $(OBJ_DIR) $(JLOS).bin $(JLOS).iso
	@echo 'clean success.'