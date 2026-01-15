# ==================john_kernel=================
JLOS := johnkernel

GPPPARAMS = -m32 -I. -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore
ASPARAMS = --32
LDPARAMS = -melf_i386

SRC_DIRS := kernel hdc drivers gui net filesystem
OBJ_DIR := obj

CPP_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
AS_SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.s))
CPP_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))
AS_OBJS := $(patsubst %.s,$(OBJ_DIR)/%.o,$(AS_SRCS))
OBJS := $(CPP_OBJS) $(AS_OBJS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@g++ $(GPPPARAMS) -o $@ -c $<

$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(@D)
	@as $(ASPARAMS) -o $@ $<

$(JLOS).bin: linker.ld  $(OBJS)
	@ld $(LDPARAMS) -T $< -o $@  $(OBJS)

install: $(JLOS).bin
	@sudo cp $< /boot/$(JLOS)/.bin

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