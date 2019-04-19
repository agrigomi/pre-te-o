.global _start
.extern _kmodule_init_
.extern _kmodule_exit_
.extern CORE_ADDR
.extern CORE_ARCH
.section ".text"
.code64

_start:
	nop
	cli
	jmp	__l1
	.byte	CORE_ARCH
	.long	CORE_ADDR 
__l1:
	pushq	8(%rsp)
	callq	_kmodule_init_
	addq	$8, %rsp
	ret
	
.global __g_core_arch__
.global __g_core_addr__

__g_core_arch__: .byte		CORE_ARCH
__g_core_addr__: .long 	CORE_ADDR


.end

