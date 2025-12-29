#ifndef __HDC_INTERUPTS_H
#define __HDC_INTERUPTS_H

#include <common/types.h>
#include <hdc/port.h>
#include <kernel/gdt.h>
#include <multitasking.h>

namespace JLOS {
namespace Hdc {
class InterruptManager;

class InterruptHandle {
public:
	virtual uint32_t HandleInterrupt(uint32_t esp);
protected:
	uint8_t interruptNumber;
	InterruptManager *interruptManager;

	InterruptHandle(uint8_t interruptNumber, InterruptManager *interruptManager);
	~InterruptHandle();
};

class InterruptManager {
friend class InterruptHandle;
protected:
	static InterruptManager *ActivateInterruptManager;
	InterruptHandle *handles[256];
	TaskManager *taskManager;

	struct GateDescriptor {
		uint16_t handleAddressLowBits;
		uint16_t gdt_codeSegmentSelector;
		uint8_t reserved;
		uint8_t access;
		uint16_t handleAddressHighBits;
	} __attribute__((packed));
	
	static GateDescriptor interruptDescriptorTable[256];
	struct InterruptDescriptorTablePointer {
		uint16_t size;
		uint32_t base;
	} __attribute__((packed));
	
	void SetInterruptDescriptorTableEntry(
		uint8_t interruptNumber,
		uint16_t codeSegmentSelectorOffset,
		void (*handler)(),
		uint8_t DescriptorPrivilegeLevel,
		uint8_t DescriptorType
	);

	Port8BitSlow picMasterCommand;
	Port8BitSlow picMasterData;
	Port8BitSlow picSlaveCommand;
	Port8BitSlow picSlaveData;

public:
	InterruptManager(uint16_t hardwareInterruptoffset, Kernel::GlobalDescriptorTable* gdt,
		TaskManager *taskManager);
	~InterruptManager();

	uint16_t hardwareInterruptOffset;

	void Activate();
	void Deactivate();
	static uint32_t handleInterrupt(uint8_t interrupt, uint32_t esp);
	uint32_t DoHandleInterrupt(uint8_t interrupt, uint32_t esp);

	static void IgnoreInterruptRequest();
	
	static void HandleException0x00();
     static void HandleException0x01();
	static void HandleException0x02();
	static void HandleException0x03();
	static void HandleException0x04();
	static void HandleException0x05();
	static void HandleException0x06();
	static void HandleException0x07();
	static void HandleException0x08();
	static void HandleException0x09();
	static void HandleException0x0A();
	static void HandleException0x0B();
	static void HandleException0x0C();
	static void HandleException0x0D();
	static void HandleException0x0E();
	static void HandleException0x0F();
	static void HandleException0x10();
	static void HandleException0x11();
	static void HandleException0x12();
	static void HandleException0x13();

	static void HandleInterruptRequest0x00();
	static void HandleInterruptRequest0x01();
	static void HandleInterruptRequest0x02();
	static void HandleInterruptRequest0x03();
	static void HandleInterruptRequest0x04();
	static void HandleInterruptRequest0x05();
	static void HandleInterruptRequest0x06();
	static void HandleInterruptRequest0x07();
	static void HandleInterruptRequest0x08();
	static void HandleInterruptRequest0x09();
	static void HandleInterruptRequest0x0A();
	static void HandleInterruptRequest0x0B();
	static void HandleInterruptRequest0x0C();
	static void HandleInterruptRequest0x0D();
	static void HandleInterruptRequest0x0E();
	static void HandleInterruptRequest0x0F();
	static void HandleInterruptRequest0x31();
	static void HandleInterruptRequest0x80();
};
}
}
#endif
