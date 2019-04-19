	BOOT_S2_SEG	= 0
	STACK		= 0x3fff
	
	.code16
	.section ".bstext", "ax"
	.global _start
	.extern main_s2

	
_start:
	jmp	$BOOT_S2_SEG,$_start2

_start2:
	mov	$0x0e32,%ax
	int	$0x10
	
	cli
	xorw	%ax, %ax
	/* setup data segments */
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	/* setup stack */
	mov	%ax, %ss
	movl	$STACK, %esp
	cld

	/* push bios device as parameter to main */
	push	%dx		
	calll	main_s2
	add	$2,%sp

	mov	$0x0e2f,%ax
	int	$0x10
	hlt
