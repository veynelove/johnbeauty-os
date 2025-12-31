GPPPARAMS = -m32 -I. -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-leading-underscore
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = obj/kernel/loader.o \
	obj/kernel/gdt.o \
	obj/hdc/port.o \
	obj/hdc/interruptstubs.o \
	obj/hdc/interrupts.o \
	obj/hdc/pci.o \
	obj/drivers/keyboard.o \
	obj/drivers/mouse.o \
	obj/drivers/driver.o \
	obj/drivers/vga.o \
	obj/drivers/amd_am79c973.o \
	obj/drivers/ata.o \
	obj/gui/desktop.o \
	obj/gui/widget.o \
	obj/gui/window.o \
	obj/kernel/multitasking.o \
	obj/kernel/memorymanagerment.o \
	obj/kernel/kernel.o \

obj/drivers/%.o: drivers/%.cpp
	mkdir -p $(@D)
	g++ ${GPPPARAMS} -o $@ -c $<

obj/hdc/%.o: hdc/%.cpp
	mkdir -p $(@D)
	g++ ${GPPPARAMS} -o $@ -c $<

obj/gui/%.o: gui/%.cpp
	mkdir -p $(@D)
	g++ ${GPPPARAMS} -o $@ -c $<

obj/kernel/%.o: kernel/%.cpp
	mkdir -p $(@D)
	g++ ${GPPPARAMS} -o $@ -c $<

obj/hdc/%.o: hdc/%.s
	mkdir -p $(@D)
	as ${ASPARAMS} -o $@ $<

obj/kernel/%.o: kernel/%.s
	mkdir -p $(@D)
	as ${ASPARAMS} -o $@ $<

johnkernel.bin: linker.ld ${objects}
	ld ${LDPARAMS} -T $< -o $@ ${objects}

install: johnkernel.bin
	sudo cp $< /boot/johnkernel.bin

johnkernel.iso: johnkernel.bin
	mkdir iso
	mkdir iso/boot
	mkdir iso/boot/grub
	cp $< iso/boot/
	echo 'set timeout=0' >> iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo '' >> iso/boot/grub/grub.cfg
	echo 'menuentry "johnbeauty oprating system" {' >> iso/boot/grub/grub.cfg
	echo ' multiboot /boot/johnkernel.bin' >> iso/boot/grub/grub.cfg
	echo ' boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue --output=$@ iso
	rm -rf iso

clean:
	rm -rf obj johnkernel.bin johnkernel.iso
	@echo 'clean success.'