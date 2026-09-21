# jlcy - JLOS C library (用户态库 + 应用程序)
ARCH := x86
OBJ_DIR := obj

CPARAMS = -m32 -I. -nostdlib -fno-builtin -fno-exceptions \
    -fno-leading-underscore -std=gnu11 \
    -ffreestanding -fno-stack-protector \
    -fno-pic -fno-pie \
    -Wall -Wextra -Wno-address-of-packed-member \
    -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx \
    -O2 -g

LDPARAMS = -melf_i386

SRC_DIRS := lib user

C_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.c")
C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS))
ASM_SRCS := $(shell find $(SRC_DIRS) -type f -name "*.S")
ASM_OBJS := $(patsubst %.S,$(OBJ_DIR)/%.o,$(ASM_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)

USER_ELF := user/hello.elf

all: $(USER_ELF)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@gcc $(CPARAMS) -o $@ -c $<

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@gcc $(CPARAMS) -o $@ -c $<

$(USER_ELF): $(OBJS) user/$(ARCH)/user_linker.ld
	@ld $(LDPARAMS) -T user/$(ARCH)/user_linker.ld -o $@ $(OBJS)

# ==================磁盘镜像生成==================
DISK_VMDK := ../disk.vmdk
DISK_FLAT := ../disk-flat.vmdk
FAT32_IMG := $(OBJ_DIR)/fat32.img

.PHONY: disk
disk: $(DISK_VMDK)

$(FAT32_IMG): $(USER_ELF)
	@mkdir -p $(@D)
	@dd if=/dev/zero of=$@ bs=512 count=30720 2>/dev/null
	@mkfs.fat -F32 $@ 2>/dev/null
	@mcopy -i $@ $(USER_ELF) ::/hello.elf

$(DISK_FLAT): $(FAT32_IMG)
	@dd if=/dev/zero of=$@ bs=512 count=32768 2>/dev/null
	@echo ',,0x0C,*' | sfdisk --quiet $@
	@dd if=$(FAT32_IMG) of=$@ bs=512 seek=2048 conv=notrunc 2>/dev/null

$(DISK_VMDK): $(DISK_FLAT)
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

clean:
	@rm -rf $(OBJ_DIR) $(USER_ELF) $(DISK_VMDK) $(DISK_FLAT)
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
		echo "    \"command\": \"gcc $(CLANGD_CPARAMS) -c $$f -o /dev/null\"," >> build/compile_commands.json; \
		echo "    \"file\": \"$$f\"" >> build/compile_commands.json; \
		echo "  }," >> build/compile_commands.json; \
	done
	@sed -i '$$ s/,$$//' build/compile_commands.json
	@echo "]" >> build/compile_commands.json
	@echo "compile_commands.json generated in build/ directory"
