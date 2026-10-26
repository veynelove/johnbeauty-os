# Copyright 2026 veyne.
# SPDX-License-Identifier: Apache-2.0

JLOS_BITS := 64
SMP ?= 4

OBJTREE_SUFFIX :=

CROSS_COMPILE ?= aarch64-linux-gnu-

ARCH_DEFINE  := -DCONFIG_ARM64
ARCH_CFLAGS  := -mcmodel=large -mgeneral-regs-only -mno-outline-atomics
ARCH_AFLAGS  :=
ARCH_LDFLAGS :=

JLOS_COMPDB_TARGET := --target=aarch64-none-elf

LDSCRIPT     := arch/arm64/linker.lds.S

TARGET_ELF   := johnkernel-arm64.elf
TARGET_IMG   := johnkernel-arm64.img
TARGET_BIN   := $(TARGET_ELF)

EXTRA_TARGETS := $(TARGET_IMG)

$(TARGET_IMG): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

QEMU_SYSTEM := $(if $(wildcard $(HOME)/qemu/bin/qemu-system-aarch64),$(HOME)/qemu/bin/qemu-system-aarch64,qemu-system-aarch64)

DISK_IMG := disk-arm64.img

.PHONY: disk
disk:
	$(MAKE) -C jlcy disk ARCH=arm64 BITS=$(JLOS_BITS)

.PHONY: run
run: $(TARGET_IMG) disk
	$(QEMU_SYSTEM) -M virt,gic-version=3 -cpu max -smp $(SMP) -kernel $(TARGET_IMG) \
		-global virtio-mmio.force-legacy=false \
		-drive if=none,file=$(DISK_IMG),format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0 \
		-netdev hubport,id=n0,hubid=0 -device virtio-net-device,netdev=n0 \
		-display none -monitor none -chardev stdio,id=s0,logfile=$(objtree)/serial.log -serial chardev:s0
