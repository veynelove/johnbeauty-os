# Copyright 2026 veyne.
# SPDX-License-Identifier: Apache-2.0

# =========================================================================
#  JohnSunshine OS 内核编译子系统（JLOS Kernel Build Subsystem）
# =========================================================================
#  用法:
#    make                      # 默认 ARCH=x86 BITS=64
#    make ARCH=x86 BITS=32     # 32 位 x86
#    make ARCH=arm64           # ARM64 交叉编译
#    make V=1                  # 回显完整编译命令（调试）
#    make run ARCH=arm64       # QEMU 启动（架构片段提供 run 目标）
#
#  交叉编译前请将工具链 bin 目录加入 PATH，或命令行覆盖 CROSS_COMPILE。
# =========================================================================

ARCH ?= x86
BITS ?= 64

BUILD_DISK ?= 1

srctree := $(CURDIR)

ifeq ($(V),1)
  quiet :=
else
  quiet := quiet_
endif

include $(srctree)/arch/$(ARCH)/Makefile

.DEFAULT_GOAL := all

objtree := out/$(ARCH)$(OBJTREE_SUFFIX)

CROSS_COMPILE ?=
CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

CFLAGS := -I$(srctree) -nostdlib -ffreestanding -fno-builtin -std=gnu2x -fno-stack-protector -fno-pic -fno-pie -Wall -Wextra -Wno-address-of-packed-member -O2 -g -Werror $(ARCH_DEFINE) $(ARCH_CFLAGS)

LDFLAGS := $(ARCH_LDFLAGS)

src-dirs := kernel arch/$(ARCH) drivers net fs hal dsa common

C_SRCS   := $(shell find $(src-dirs) -type f -name '*.c')
ASM_SRCS := $(shell find $(src-dirs) -type f -name '*.S')
ASM_SRCS := $(filter-out $(LDSCRIPT),$(ASM_SRCS))

C_OBJS   := $(patsubst %.c,$(objtree)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.S,$(objtree)/%.o,$(ASM_SRCS))
OBJS     := $(C_OBJS) $(ASM_OBJS)

LDSCRIPT_BIN := $(objtree)/linker.lds

quiet_cmd_cc_o_c =  CC      $@
      cmd_cc_o_c = $(CC) $(CFLAGS) -c -o $@ $<

quiet_cmd_as_o_S =  AS      $@
      cmd_as_o_S = $(CC) $(CFLAGS) -x assembler-with-cpp -c -o $@ $<

quiet_cmd_lds    =  LDS     $@
      cmd_lds    = $(CC) $(ARCH_DEFINE) -E -P -x assembler-with-cpp -o $@ $<

quiet_cmd_link   =  LD      $@
      cmd_link   = $(LD) $(LDFLAGS) -T $(LDSCRIPT_BIN) -o $@ $(OBJS)

cmd = @set -e; \
	$(if $($(quiet)cmd_$(1)), echo '  $($(quiet)cmd_$(1))';) \
	$(cmd_$(1))

all: banner
	@t0=$$(date +%s); \
	$(MAKE) -s --no-print-directory $(TARGET_BIN) $(EXTRA_TARGETS) && \
		{ echo '  ============================================================'; \
		  echo "  BUILD OK    : $$(($$(date +%s)-t0))s"; \
		  echo '  ============================================================'; } || \
		{ echo '  ============================================================'; \
		  echo "  BUILD FAIL  : $$(($$(date +%s)-t0))s"; \
		  echo '  ============================================================'; exit 1; }

.PHONY: banner
banner:
	@echo ''
	@echo '  ============================================================'
	@echo '  JohnSunshine OS Kernel  (JLOS)'
	@echo '  ============================================================'
	@echo "   TARGET   : $(TARGET_BIN)"
	@echo "   ARCH     : $(ARCH)  ($(JLOS_BITS)-bit)"
	@echo "   CC       : $(shell $(CC) --version | head -n 1)"
	@echo "   CFLAGS   : $(CFLAGS)"
	@echo '  ============================================================'
	@echo ''

$(LDSCRIPT_BIN): $(LDSCRIPT)
	@mkdir -p $(@D)
	$(call cmd,lds)

$(objtree)/%.o: %.c
	@mkdir -p $(@D)
	$(call cmd,cc_o_c)

$(objtree)/%.o: %.S
	@mkdir -p $(@D)
	$(call cmd,as_o_S)

$(TARGET_BIN): $(LDSCRIPT_BIN) $(OBJS)
	$(call cmd,link)

.PHONY: clean
clean:
	@echo 'clean artifacts'
	@rm -rf out iso
	@rm -f johnkernel-*.bin johnkernel-*.iso johnkernel-*.elf johnkernel-*.img
	@rm -f disk-arm64.img disk.img disk.vmdk disk-flat.vmdk
	@$(MAKE) -s --no-print-directory -C jlcy clean ARCH=$(ARCH) BITS=$(JLOS_BITS)

.PHONY: install
install: $(TARGET_BIN)
	@sudo cp $(TARGET_BIN) /boot/$(TARGET_BIN)

COMPDB_DIR := build/$(ARCH)$(OBJTREE_SUFFIX)

.PHONY: compdb
compdb:
	@echo '  GEN     $(COMPDB_DIR)/compile_commands.json'
	@mkdir -p $(COMPDB_DIR)
	@{ \
		echo '['; \
		first=1; \
		for f in $(C_SRCS); do \
			if [ $$first -eq 1 ]; then first=0; else echo ','; fi; \
			printf '  { "directory": "%s", "command": "%s %s %s -c %s -o /dev/null", "file": "%s" }\n' \
				"$(srctree)" "$(CC)" "$(CFLAGS)" "$(JLOS_COMPDB_TARGET)" "$$f" "$$f"; \
		done; \
		echo ']'; \
	} > $(COMPDB_DIR)/compile_commands.json
	@echo '  DONE    clangd database ready'