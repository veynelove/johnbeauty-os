# Copyright 2026 veyne.
# SPDX-License-Identifier: Apache-2.0

# ==================john_kernel=================
JLOS := johnkernel
ARCH := x86

BITS ?= 64

# 磁盘镜像构建开关（1=构建, 0=跳过）
BUILD_DISK ?= 1

ifeq ($(BITS), 64)
BITS_CFLAGS 	= -m64 -mcmodel=kernel -mno-red-zone
BITS_DEFINE		= -DCONFIG_X86_64
BITS_SSE		= -mgeneral-regs-only
ASFLAGS_ARCH 	= --64
LDFLAGS_ARCH 	= -melf_x86_64
JLOS_BIN		= $(JLOS)64.bin
JLOS_ISO		= $(JLOS)64.iso
else
BITS_CFLAGS		= -m32
BITS_DEFINE		= -DCONFIG_X86_32
BITS_SSE		= -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
ASFLAGS_ARCH	= --32
LDFLAGS_ARCH	= -melf_i386
JLOS_BIN		= $(JLOS).bin
JLOS_ISO		= $(JLOS).iso
endif

ifeq ($(BUILD_DISK),1)
all: $(JLOS_ISO) disk
else
all: $(JLOS_ISO)
endif

# 实际编译用的完整参数
CPARAMS = $(BITS_CFLAGS) $(BITS_DEFINE) -I. -nostdlib -fno-builtin -fno-exceptions \
		-fno-leading-underscore -std=gnu2x \
		-ffreestanding -fno-stack-protector \
		-fno-pic -fno-pie \
		-Wall -Wextra -Wno-address-of-packed-member \
		$(BITS_SSE) -O2 -g

# clangd 专用参数（去掉 -fno-leading-underscore，clangd 不认识它）
CLANGD_CPARAMS = $(filter-out -fno-leading-underscore,$(CPARAMS))

ASPARAMS = $(ASFLAGS_ARCH) -I.
LDPARAMS = $(LDFLAGS_ARCH)

SRC_DIRS := kernel arch/$(ARCH) drivers net fs hal dsa common
OBJ_DIR := obj

C_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.c")
LD_SCRIPT_SRC := arch/$(ARCH)/linker.lds.S
AS_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.s") $(shell find $(SRC_DIRS) -type f -name "*.S")
AS_SRCS := $(filter-out $(LD_SCRIPT_SRC),$(AS_SRCS))
C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS))
AS_OBJS := $(patsubst %.s,$(OBJ_DIR)/%.o,$(filter %.s,$(AS_SRCS))) $(patsubst %.S,$(OBJ_DIR)/%.o,$(filter %.S,$(AS_SRCS)))
OBJS := $(C_OBJS) $(AS_OBJS)

# 实际编译用 CPARAMS（完整参数）
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@gcc $(CPARAMS) -o $@ -c $<

$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(@D)
	@as $(ASPARAMS) -o $@ $<

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@gcc $(CPARAMS) -x assembler-with-cpp -o $@ -c $<

$(OBJ_DIR)/linker.lds: arch/$(ARCH)/linker.lds.S
	@mkdir -p $(@D)
	@gcc $(BITS_DEFINE) -E -P -x assembler-with-cpp -o $@ $<

$(JLOS_BIN): $(OBJ_DIR)/linker.lds $(OBJS)
	@ld $(LDPARAMS) -T $(OBJ_DIR)/linker.lds -o $(JLOS_BIN) $(OBJS)

install: $(JLOS_BIN)
	@sudo cp $< /boot/$(JLOS_BIN)

$(JLOS_ISO): $(JLOS_BIN)
	@rm -rf iso
	@mkdir -p iso/boot/grub
	@cp $< iso/boot/$(JLOS_BIN)
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo '' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "johnlove operating system" {' >> iso/boot/grub/grub.cfg
	@echo ' multiboot /boot/$(JLOS_BIN)' >> iso/boot/grub/grub.cfg
	@echo ' boot' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	@grub-mkrescue --output=$(JLOS_ISO) iso
	@rm -rf iso

# ==================用户态程序（jlcy 子项目）==================
.PHONY: disk
disk:
	@$(MAKE) -C jlcy disk BITS=$(BITS)

clean:
	@rm -rf $(OBJ_DIR) $(JLOS).bin $(JLOS)64.bin $(JLOS).iso $(JLOS)64.iso
	@$(MAKE) -C jlcy clean
	@echo 'clean success.'

# ==================clangd 支持==================
# 生成 compile_commands.json 到 build/ 目录（使用 CLANGD_CPARAMS，去掉了不支持的参数）
.PHONY: compdb
compdb:
	@mkdir -p build
	@echo "[" > build/compile_commands.json
	@for f in $(C_SRCS); do \
		echo "  {" >> build/compile_commands.json; \
		echo "    \"directory\": \"$(shell pwd)\"," >> build/compile_commands.json; \
		echo "    \"command\": \"gcc $(CLANGD_CPARAMS) -c $$f -o /dev/null\"," >> build/compile_commands.json; \
		echo "    \"file\": \"$$f\"" >> build/compile_commands.json; \
		echo "  }," >> build/compile_commands.json; \
	done
	@sed -i '$$ s/,$$//' build/compile_commands.json
	@echo "]" >> build/compile_commands.json
	@echo "compile_commands.json generated in build/ directory"
