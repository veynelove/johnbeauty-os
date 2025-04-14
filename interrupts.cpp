#include "interrupts.h"

void printf(const char *str);
void printfHex(uint8_t);

InterruptHandle::InterruptHandle(uint8_t interruptNumber, InterruptManager *interruptManager)
{
	this->interruptNumber = interruptNumber;
	this->interruptManager = interruptManager;
	interruptManager->handles[interruptNumber] = this;
}

InterruptHandle::~InterruptHandle()
{
	if(interruptManager->handles[interruptNumber] == this)
		interruptManager->handles[interruptNumber] = nullptr;
}

uint32_t InterruptHandle::HandleInterrupt(uint32_t esp)
{
	return esp;
}

InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[256];
InterruptManager* InterruptManager::ActivateInterruptManager = nullptr;

void InterruptManager::SetInterruptDescriptorTableEntry(
				uint8_t interruptNumber,
				uint16_t codeSegmentSelectorOffset,
				void (*handler)(),
				uint8_t DescriptorPrivilegeLevel,
				uint8_t DescriptorType)
{
	const uint8_t IDT_DESC_PRESENT = 0x80;
	
	interruptDescriptorTable[interruptNumber].handleAddressLowBits = ((uint32_t)handler) & 0xFFFF;
	interruptDescriptorTable[interruptNumber].handleAddressHighBits = (((uint32_t)handler) >>16) & 0xFFFF;
	interruptDescriptorTable[interruptNumber].gdt_codeSegmentSelector = codeSegmentSelectorOffset;
	interruptDescriptorTable[interruptNumber].access = IDT_DESC_PRESENT | DescriptorType | ((DescriptorPrivilegeLevel&3) <<5);
	interruptDescriptorTable[interruptNumber].reserved = 0;
}
		
InterruptManager::InterruptManager(GlobalDescriptorTable* gdt)
:picMasterCommand(0x20), picMasterData(0x21), picSlaveCommand(0xA0), picSlaveData(0xA1)
{
	uint16_t CodeSegment = gdt->CodeSegmentSelector();
	const uint8_t IDT_INTERRUPT_GATE = 0XE;
	for(uint16_t i=0; i<256;i++) {
		handles[i] = nullptr;
		SetInterruptDescriptorTableEntry(i, CodeSegment, &IgnoreInterruptRequest, 0, IDT_INTERRUPT_GATE);
	}
	SetInterruptDescriptorTableEntry(0x20, CodeSegment, &HandleInterruptRequest0x00, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x21, CodeSegment, &HandleInterruptRequest0x01, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x2C, CodeSegment, &HandleInterruptRequest0x0C, 0, IDT_INTERRUPT_GATE);

	picMasterCommand.Write(0x11);
	picSlaveCommand.Write(0x11);
	picMasterData.Write(0x20);
	picSlaveData.Write(0x28); 
	picMasterData.Write(0x04);
	picSlaveData.Write(0x02); 
	picMasterData.Write(0x01);
	picSlaveData.Write(0x01); 
	picMasterData.Write(0x00);
	picSlaveData.Write(0x00); 

	InterruptDescriptorTablePointer idt;
	idt.size = 256 * sizeof(GateDescriptor) -1;
	idt.base = (uint32_t)interruptDescriptorTable;
	asm volatile("lidt %0" : : "m" (idt));
}

InterruptManager::~InterruptManager()
{
}

void InterruptManager::Activate()
{
	if(ActivateInterruptManager != nullptr)
		ActivateInterruptManager->Deactivate();
	ActivateInterruptManager = this;
	asm("sti");
}

void InterruptManager::Deactivate()
{
	if(ActivateInterruptManager == this) {
		ActivateInterruptManager = nullptr;
		asm("cli");
	}
}

uint32_t InterruptManager::handleInterrupt(uint8_t interruptNumber, uint32_t esp)
{
	if(ActivateInterruptManager != nullptr)
		return ActivateInterruptManager->DoHandleInterrupt(interruptNumber, esp);
    return esp;
}

uint32_t InterruptManager::DoHandleInterrupt(uint8_t interruptNumber, uint32_t esp)
{
	if(handles[interruptNumber] != nullptr) {
		esp = handles[interruptNumber]->HandleInterrupt(esp);
	}
	else if(interruptNumber != 0x20) {
		printf("UNHANDLED INTERUPT 0x");
		printfHex(interruptNumber);
	}
		
	if(0x20<=interruptNumber && interruptNumber<0x30) {
		picMasterCommand.Write(0x20);
		if(0x28<=interruptNumber)
			picSlaveCommand.Write(0x20);
	}
    return esp;
}
