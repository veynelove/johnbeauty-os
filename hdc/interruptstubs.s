.set IRQ_BASE, 0X20
.section .text

.extern _ZN4JLOS3Hdc16InterruptManager15handleInterruptEhj

.global _ZN4JLOS3Hdc16InterruptManager22IgnoreInterruptRequestEv

.macro HandleException num
.global _ZN4JLOS3Hdc16InterruptManager16HandleException\num\()Ev
_ZN4JLOS3Hdc16InterruptManager16HandleException\num\()Ev:
	movb $\num, (interruptnumber)
	jmp int_bottom
.endm

.macro HandleInterruptRequest num
.global _ZN4JLOS3Hdc16InterruptManager26HandleInterruptRequest\num\()Ev
_ZN4JLOS3Hdc16InterruptManager26HandleInterruptRequest\num\()Ev:
	movb $\num + IRQ_BASE, (interruptnumber)
	jmp int_bottom
.endm

HandleInterruptRequest 0x00
HandleInterruptRequest 0x01
HandleInterruptRequest 0x0C

int_bottom:
	pusha
	pushl %ds
	pushl %es
	pushl %fs
	pushl %gs
	
	pushl %esp
	push (interruptnumber)
	call _ZN4JLOS3Hdc16InterruptManager15handleInterruptEhj
	# addl $5, %esp
	movl %eax, %esp
	
	popl %gs
	popl %fs
	popl %es
	popl %ds
	popa

_ZN4JLOS3Hdc16InterruptManager22IgnoreInterruptRequestEv:
	iret

.data
	interruptnumber: .byte 0
