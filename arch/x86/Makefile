# Copyright 2026 veyne.
# SPDX-License-Identifier: Apache-2.0

JLOS_BITS := $(BITS)

ifeq ($(JLOS_BITS),64)
  ARCH_DEFINE  := -DCONFIG_X86_64
  ARCH_CFLAGS  := -m64 -mcmodel=kernel -mno-red-zone -mgeneral-regs-only
  ARCH_LDFLAGS := -m elf_x86_64
  TARGET_BIN   := johnkernel-x86_64.bin
  TARGET_ISO   := johnkernel-x86_64.iso
else ifeq ($(JLOS_BITS),32)
  ARCH_DEFINE  := -DCONFIG_X86_32
  ARCH_CFLAGS  := -m32 -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
  ARCH_LDFLAGS := -m elf_i386
  TARGET_BIN   := johnkernel-x86.bin
  TARGET_ISO   := johnkernel-x86.iso
else
  $(error not support x86 bit width: BITS=$(BITS), you can select 32/64)
endif

ARCH_AFLAGS :=
LDSCRIPT    := arch/x86/linker.lds.S

EXTRA_TARGETS := $(TARGET_ISO)
ifeq ($(BUILD_DISK),1)
  EXTRA_TARGETS += disk
endif

$(TARGET_ISO): $(TARGET_BIN)
	@rm -rf iso
	@mkdir -p iso/boot/grub
	@cp $(TARGET_BIN) iso/boot/$(TARGET_BIN)
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo '' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "johnlove operating system" {' >> iso/boot/grub/grub.cfg
	@echo '  multiboot /boot/$(TARGET_BIN)' >> iso/boot/grub/grub.cfg
	@echo '  boot' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	@grub-mkrescue --output=$(TARGET_ISO) iso
	@rm -rf iso

.PHONY: disk
disk:
	@$(MAKE) -C jlcy disk BITS=$(JLOS_BITS)