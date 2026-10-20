# Copyright 2026 veyne.
# SPDX-License-Identifier: Apache-2.0

JLOS_BITS := 64

CROSS_COMPILE ?= aarch64-none-elf-

ARCH_DEFINE  := -DCONFIG_ARM64
ARCH_CFLAGS  := -mcmodel=large -mgeneral-regs-only -mno-outline-atomics
ARCH_AFLAGS  :=
ARCH_LDFLAGS :=

LDSCRIPT     := arch/arm64/linker.lds.S
TARGET_BIN   := johnkernel-arm64.bin
EXTRA_TARGETS :=

.PHONY: run
run: $(TARGET_BIN)
	qemu-system-aarch64 -machine virt -cpu cortex-a72 -kernel $(TARGET_BIN) -nographic