# ==================john_kernel=================
JLOS := johnkernel
ARCH := x86

all: $(JLOS).iso

# 实际编译用的完整参数
CPARAMS = -m32 -I. -nostdlib -fno-builtin -fno-exceptions \
		-fno-leading-underscore -std=gnu11 \
		-ffreestanding -fno-stack-protector \
		-fno-pic -fno-pie \
		-Wall -Wextra -Wno-address-of-packed-member \
		-mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx \
		-O2 -g

# clangd 专用参数（去掉 -fno-leading-underscore，clangd 不认识它）
CLANGD_CPARAMS = $(filter-out -fno-leading-underscore,$(CPARAMS))

ASPARAMS = --32 -I.
LDPARAMS = -melf_i386

SRC_DIRS := kernel arch/$(ARCH) drivers net filesystem tools hal dsa common lib
OBJ_DIR := obj

C_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.c")
AS_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.s")
C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS))
AS_OBJS := $(patsubst %.s,$(OBJ_DIR)/%.o,$(AS_SRCS))
OBJS := $(C_OBJS) $(AS_OBJS)

# 实际编译用 CPARAMS（完整参数）
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@gcc $(CPARAMS) -o $@ -c $<

$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(@D)
	@as $(ASPARAMS) -o $@ $<

$(JLOS).bin: arch/$(ARCH)/linker.ld  $(OBJS)
	@ld $(LDPARAMS) -T $< -o $(JLOS).bin $(OBJS)

install: $(JLOS).bin
	@sudo cp $< /boot/$(JLOS).bin

$(JLOS).iso: $(JLOS).bin
	@rm -rf iso
	@mkdir -p iso/boot/grub
	@cp $< iso/boot/$(JLOS).bin
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo '' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "johnlove operating system" {' >> iso/boot/grub/grub.cfg
	@echo ' multiboot /boot/$(JLOS).bin' >> iso/boot/grub/grub.cfg
	@echo ' boot' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	@grub-mkrescue --output=$@ iso
	@rm -rf iso

clean:
	@rm -rf $(OBJ_DIR) $(JLOS).bin $(JLOS).iso
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