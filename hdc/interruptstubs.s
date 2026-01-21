.set IRQ_BASE, 0X20
.section .text

.extern _ZN4JLOS3Hdc17interrupt_manager16handle_interruptEhj

.global _ZN4JLOS3Hdc17interrupt_manager24ignore_interrupt_requestEv

.macro handle_exception num
.global _ZN4JLOS3Hdc17interrupt_manager20handle_exception\num\()Ev
_ZN4JLOS3Hdc17interrupt_manager20handle_exception\num\()Ev:
	movb $\num, (interruptnumber)
	jmp int_bottom
.endm

.macro handle_interrupt_request num
.global _ZN4JLOS3Hdc17interrupt_manager28handle_interrupt_request\num\()Ev
_ZN4JLOS3Hdc17interrupt_manager28handle_interrupt_request\num\()Ev:
	movb $\num + IRQ_BASE, (interruptnumber)
	pushl $0
	jmp int_bottom
.endm

handle_exception 0x00
handle_exception 0x01
handle_exception 0x02
handle_exception 0x03
handle_exception 0x04
handle_exception 0x05
handle_exception 0x06
handle_exception 0x07
handle_exception 0x08
handle_exception 0x09
handle_exception 0x0a
handle_exception 0x0b
handle_exception 0x0c
handle_exception 0x0d
handle_exception 0x0e
handle_exception 0x0f
handle_exception 0x10
handle_exception 0x11
handle_exception 0x12
handle_exception 0x13

handle_interrupt_request 0x00
handle_interrupt_request 0x01
handle_interrupt_request 0x02
handle_interrupt_request 0x03
handle_interrupt_request 0x04
handle_interrupt_request 0x05
handle_interrupt_request 0x06
handle_interrupt_request 0x07
handle_interrupt_request 0x08
handle_interrupt_request 0x09
handle_interrupt_request 0x0a
handle_interrupt_request 0x0b
handle_interrupt_request 0x0c
handle_interrupt_request 0x0d
handle_interrupt_request 0x0e
handle_interrupt_request 0x0f
handle_interrupt_request 0x31

handle_interrupt_request 0x80

int_bottom:
	# save registers
	#pusha
	#pushl %ds
	#pushl %es
	#pushl %fs
	#pushl %gs

	pushl %ebp
	pushl %edi
	pushl %esi
    
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	# load ring 0 segent register
	#cld
	#mov $0x10, %eax
	#mov %eax, %eds
	#mov %eax, %ees

	# call c++ handler
	pushl %esp
	push (interruptnumber)
	call _ZN4JLOS3Hdc17interrupt_manager16handle_interruptEhj
	#add %esp, 6
	mov %eax, %esp # switch the stack
    
	# restore registers
	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
    
		popl %esi
		popl %edi
		popl %ebp

		#pop %gs
		#pop %fs
		#pop %es
		#pop %ds
		#popa

		add $4, %esp

	.global _ZN4JLOS3Hdc17interrupt_manager16interrupt_ignoreEv;
	_ZN4JLOS3Hdc17interrupt_manager24ignore_interrupt_requestEv:
		iret

	.data
		interruptnumber: .byte 0
