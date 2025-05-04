#ifndef __HDC_INTERUPTS_H
#define __HDC_INTERUPTS_H

#include <common/types.h>
#include <hdc/port.h>
#include <kernel/gdt.h>

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
public:
	InterruptManager(JLOS::Kernel::GlobalDescriptorTable* gdt);
	~InterruptManager();

	void Activate();
	void Deactivate();

	static uint32_t handleInterrupt(uint8_t interruptNumber, uint32_t esp);
	uint32_t DoHandleInterrupt(uint8_t interruptNumber, uint32_t esp);

	static void IgnoreInterruptRequest();
	static void HandleInterruptRequest0x00();
	static void HandleInterruptRequest0x01();
	static void HandleInterruptRequest0x0C();
protected:
	static InterruptManager *ActivateInterruptManager;
	InterruptHandle *handles[256];

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
};
}
}
#endif
