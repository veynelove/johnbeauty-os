# Copyright 2026 veyne.
# SPDX-License-Identifier: Apache-2.0

# jlcy - JLOS C library (用户态库 + 应用程序)
ARCH ?= x86
BITS ?= 64

OBJ_DIR := obj/$(ARCH)

CC := gcc
LD := ld
FLOAT_FLAGS :=

ifeq ($(ARCH), arm64)
CC := aarch64-linux-gnu-gcc
LD := aarch64-linux-gnu-ld
BITS_CFLAGS  := -mcmodel=small
BITS_DEFINE  := -DCONFIG_ARM64
LDPARAMS     :=
LINKER_LD    := user_linker64.ld

else ifeq ($(ARCH), x86)
ifeq ($(BITS), 64)
BITS_CFLAGS  = -m64
BITS_DEFINE = -DCONFIG_X86_64
LDPARAMS    = -melf_x86_64
LINKER_LD   = user_linker64.ld
else
BITS_CFLAGS  = -m32
BITS_DEFINE = -DCONFIG_X86_32
LDPARAMS    = -melf_i386
LINKER_LD   = user_linker.ld
endif
FLOAT_FLAGS = -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
else
$(error unsupported arch: $(ARCH))
endif

CPARAMS = $(BITS_CFLAGS) $(BITS_DEFINE) -I. -nostdlib -fno-builtin -fno-exceptions \
    -fno-leading-underscore -std=gnu2x \
    -ffreestanding -fno-stack-protector \
    -fno-pic -fno-pie \
    -Wall -Wextra -Wno-address-of-packed-member \
    $(FLOAT_FLAGS) \
    -O2 -g

LIB_C_SRCS := $(shell find lib -maxdepth 1 -type f -name "*.c") \
              $(shell find lib/$(ARCH) -type f -name "*.c" 2>/dev/null)
LIB_ASM_SRCS := $(shell find lib/$(ARCH) -type f -name "*.S" 2>/dev/null)
LIB_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIB_C_SRCS)) \
            $(patsubst %.S,$(OBJ_DIR)/%.o,$(LIB_ASM_SRCS))

USER_C_SRCS := $(shell find user -type f -name "*.c")
USER_ELFS := $(patsubst user/%.c,user/%.elf,$(USER_C_SRCS))

C_SRCS := $(LIB_C_SRCS) $(USER_C_SRCS)
ALL_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS)) $(patsubst %.S,$(OBJ_DIR)/%.o,$(LIB_ASM_SRCS))

all: $(USER_ELFS)

.SECONDARY: $(ALL_OBJS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@$(CC) $(CPARAMS) -o $@ -c $<

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@$(CC) $(CPARAMS) -o $@ -c $<

user/%.elf: $(OBJ_DIR)/user/%.o $(LIB_OBJS) user/$(ARCH)/$(LINKER_LD)
	@$(LD) $(LDPARAMS) -T user/$(ARCH)/$(LINKER_LD) -o $@ $< $(LIB_OBJS)

# ==================磁盘镜像生成==================
FAT32_IMG := $(OBJ_DIR)/fat32.img
PART_IMG := $(OBJ_DIR)/disk-part.img

ifeq ($(ARCH), arm64)
DISK_OUT := ../disk-arm64.img
else
DISK_OUT := ../disk.vmdk
DISK_FLAT := ../disk-flat.vmdk
endif

.PHONY: disk
disk: $(DISK_OUT)

$(FAT32_IMG): $(USER_ELFS)
	@mkdir -p $(@D)
	@dd if=/dev/zero of=$@ bs=512 count=30720 2>/dev/null
	@mkfs.fat -F32 $@ 2>/dev/null
	@for elf in $(USER_ELFS); do mcopy -i $@ $$elf ::/$${elf##*/} 2>/dev/null; done

$(PART_IMG): $(FAT32_IMG)
	@dd if=/dev/zero of=$@ bs=512 count=32768 2>/dev/null
	@echo ',,0x0C,*' | sfdisk --quiet $@
	@dd if=$(FAT32_IMG) of=$@ bs=512 seek=2048 conv=notrunc 2>/dev/null

ifeq ($(ARCH), arm64)
$(DISK_OUT): $(PART_IMG)
	@cp $(PART_IMG) $@
else
$(DISK_FLAT): $(PART_IMG)
	@cp $(PART_IMG) $@

$(DISK_OUT): $(DISK_FLAT)
	@echo '# Disk DescriptorFile' > $@
	@echo 'version=1' >> $@
	@echo 'CID=ffffff01' >> $@
	@echo 'parentCID=FFFFFFFF' >> $@
	@echo 'createType="monolithicFlat"' >> $@
	@echo '' >> $@
	@echo '# Extent description' >> $@
	@echo 'RW 32768 FLAT "disk-flat.vmdk" 0' >> $@
	@echo '' >> $@
	@echo '# The Disk Data Base' >> $@
	@echo '#DDB' >> $@
	@echo '' >> $@
	@echo 'ddb.virtualHWVersion = "4"' >> $@
	@echo 'ddb.geometry.cylinders = "32"' >> $@
	@echo 'ddb.geometry.heads = "16"' >> $@
	@echo 'ddb.geometry.sectors = "63"' >> $@
	@echo 'ddb.adapterType = "ide"' >> $@
endif

clean:
	@rm -rf obj user/*.elf $(DISK_OUT) $(DISK_FLAT)
	@echo 'clean success.'

# ==================clangd 支持==================
CLANGD_CPARAMS = $(filter-out -fno-leading-underscore,$(CPARAMS))

.PHONY: compdb
compdb:
	@mkdir -p build
	@echo "[" > build/compile_commands.json
	@for f in $(C_SRCS); do \
		echo "  {" >> build/compile_commands.json; \
		echo "    \"directory\": \"$(shell pwd)\"," >> build/compile_commands.json; \
		echo "    \"command\": \"$(CC) $(CLANGD_CPARAMS) -c $$f -o /dev/null\"," >> build/compile_commands.json; \
		echo "    \"file\": \"$$f\"" >> build/compile_commands.json; \
		echo "  }," >> build/compile_commands.json; \
	done
	@sed -i '$$ s/,$$//' build/compile_commands.json
	@echo "]" >> build/compile_commands.json
	@echo "compile_commands.json generated in build/ directory"
