#include <hdc/interrupts.h>

namespace JLOS::Kernel {
void printf(const char *str);
void printfHex(uint8_t);
}

namespace JLOS {
namespace Hdc {
InterruptHandle::InterruptHandle(uint8_t interruptNumber, InterruptManager *interruptManager)
{
	this->interruptNumber = interruptNumber;
	this->interruptManager = interruptManager;
	interruptManager->handles[interruptNumber] = this;
}

InterruptHandle::~InterruptHandle()
{
	if (interruptManager->handles[interruptNumber] == this) {
		interruptManager->handles[interruptNumber] = nullptr;
	}
}

uint32_t InterruptHandle::HandleInterrupt(uint32_t esp)
{
	return esp;
}

InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[256];
InterruptManager* InterruptManager::ActivateInterruptManager = nullptr;

void InterruptManager::SetInterruptDescriptorTableEntry(uint8_t interruptNumber,
	uint16_t codeSegmentSelectorOffset, void (*handler)(), uint8_t DescriptorPrivilegeLevel,
	uint8_t DescriptorType)
{
	const uint8_t IDT_DESC_PRESENT = 0x80;
	
	interruptDescriptorTable[interruptNumber].handleAddressLowBits =
		((uint32_t)handler) & 0xFFFF;
	interruptDescriptorTable[interruptNumber].handleAddressHighBits =
		(((uint32_t)handler) >>16) & 0xFFFF;
	interruptDescriptorTable[interruptNumber].gdt_codeSegmentSelector =
		codeSegmentSelectorOffset;
	interruptDescriptorTable[interruptNumber].access = (IDT_DESC_PRESENT | DescriptorType
		| ((DescriptorPrivilegeLevel&3) <<5));
	interruptDescriptorTable[interruptNumber].reserved = 0;
}
		
InterruptManager::InterruptManager(uint16_t hardwareInterruptoffset,
	Kernel::GlobalDescriptorTable* gdt, TaskManager *taskManager) : picMasterCommand(0x20),
	picMasterData(0x21), picSlaveCommand(0xA0), picSlaveData(0xA1)
{
	this->taskManager = taskManager;
	this->hardwareInterruptOffset = hardwareInterruptoffset;
	uint16_t CodeSegment = gdt->CodeSegmentSelector();
	const uint8_t IDT_INTERRUPT_GATE = 0XE;
	for (uint16_t i = 0; i < 256; i++) {
		handles[i] = nullptr;
		SetInterruptDescriptorTableEntry(i, CodeSegment, &IgnoreInterruptRequest, 0,
			IDT_INTERRUPT_GATE);
	}
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset, CodeSegment,
		&HandleInterruptRequest0x00, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x01, CodeSegment,
		&HandleInterruptRequest0x01, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x02, CodeSegment,
		&HandleInterruptRequest0x02, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x03, CodeSegment,
		&HandleInterruptRequest0x03, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x04, CodeSegment,
		&HandleInterruptRequest0x04, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x05, CodeSegment,
		&HandleInterruptRequest0x05, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x06, CodeSegment,
		&HandleInterruptRequest0x06, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x07, CodeSegment,
		&HandleInterruptRequest0x07, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x08, CodeSegment,
		&HandleInterruptRequest0x08, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x09, CodeSegment,
		&HandleInterruptRequest0x09, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0A, CodeSegment,
		&HandleInterruptRequest0x0A, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0B, CodeSegment,
		&HandleInterruptRequest0x0B, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0C, CodeSegment,
		&HandleInterruptRequest0x0C, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0D, CodeSegment,
		&HandleInterruptRequest0x0D, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0E, CodeSegment,
		&HandleInterruptRequest0x0E, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0F, CodeSegment,
		&HandleInterruptRequest0x0F, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x31, CodeSegment,
		&HandleInterruptRequest0x31, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(					    0x80, CodeSegment,
		&HandleInterruptRequest0x80, 0, IDT_INTERRUPT_GATE);

	SetInterruptDescriptorTableEntry(0x00, CodeSegment, &HandleException0x00, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x01, CodeSegment, &HandleException0x01, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x02, CodeSegment, &HandleException0x02, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x03, CodeSegment, &HandleException0x03, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x04, CodeSegment, &HandleException0x04, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x05, CodeSegment, &HandleException0x05, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x06, CodeSegment, &HandleException0x06, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x07, CodeSegment, &HandleException0x07, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x08, CodeSegment, &HandleException0x08, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x09, CodeSegment, &HandleException0x09, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x0A, CodeSegment, &HandleException0x0A, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x0B, CodeSegment, &HandleException0x0B, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x0C, CodeSegment, &HandleException0x0C, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x0D, CodeSegment, &HandleException0x0D, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x0E, CodeSegment, &HandleException0x0E, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x0F, CodeSegment, &HandleException0x0F, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x10, CodeSegment, &HandleException0x10, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x11, CodeSegment, &HandleException0x11, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x12, CodeSegment, &HandleException0x12, 0, IDT_INTERRUPT_GATE);
	SetInterruptDescriptorTableEntry(0x13, CodeSegment, &HandleException0x13, 0, IDT_INTERRUPT_GATE);

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

InterruptManager::~InterruptManager(){}

void InterruptManager::Activate()
{
	if (ActivateInterruptManager != nullptr) {
		ActivateInterruptManager->Deactivate();
	}
	ActivateInterruptManager = this;
	asm("sti");
}

void InterruptManager::Deactivate()
{
	if (ActivateInterruptManager == this) {
		ActivateInterruptManager = nullptr;
		asm("cli");
	}
}

uint32_t InterruptManager::handleInterrupt(uint8_t interrupt, uint32_t esp)
{
	if (ActivateInterruptManager != nullptr) {
		return ActivateInterruptManager->DoHandleInterrupt(interrupt, esp);
	}
    return esp;
}

uint32_t InterruptManager::DoHandleInterrupt(uint8_t interrupt, uint32_t esp)
{
	if (handles[interrupt] != nullptr) {
		esp = handles[interrupt]->HandleInterrupt(esp);
	}
	else if (interrupt != hardwareInterruptOffset) {
		Kernel::printf("UNHANDLED INTERUPT 0x");
		Kernel::printfHex(interrupt);
	}
	
	if (interrupt == hardwareInterruptOffset) {
		esp = (uint32_t)taskManager->Schedule((CPUState *)esp);
	}
	//hardware interrupts must be acknowledged
	if (hardwareInterruptOffset <= interrupt && interrupt < hardwareInterruptOffset + 16) {
		picMasterCommand.Write(0x20);
		if (hardwareInterruptOffset + 8 <= interrupt)
			picSlaveCommand.Write(0x20);
	}
    return esp;
}
}
}
