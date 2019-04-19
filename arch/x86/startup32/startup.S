.global _start
.extern _init
.extern _kentry
.section ".bstext", "ax"

.code32
_start:
	nop
	nop
	jmp	__l1
	.byte	0x32
__l1:	
	call	_init
	pushl	4(%esp)		/* startup context */
	call	_kentry
	addl	$4,%esp
	ret
.end
