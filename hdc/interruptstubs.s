.set IRQ_BASE, 0X20
.section .text

.extern jlos_interrupt_manager_handle_interrupt

.global jlos_ignore_interrupt_request

.macro handle_exception num
.global jlos_handle_exception\num\()
jlos_handle_exception\num\():
	movb $\num, (interruptnumber)
	jmp int_bottom
.endm

.macro handle_interrupt_request num
.global jlos_handle_interrupt_request\num\()
jlos_handle_interrupt_request\num\():
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

	pushl %ebp
	pushl %edi
	pushl %esi
    
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	pushl %esp
	push (interruptnumber)
	call jlos_interrupt_manager_handle_interrupt
	mov %eax, %esp

	# restore registers
	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
    
	popl %esi
	popl %edi
	popl %ebp

	add $4, %esp
 
	jlos_ignore_interrupt_request:
		iret

	.data
		interruptnumber: .long 0
