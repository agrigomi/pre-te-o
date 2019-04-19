;WARNING: This code will be changed for HDD boot. Now it's support FDD only 

;------------------------------------------------------------
CPU 286						   ; CPU type
;------------------------------------------------------------
SEGMENT .text
BITS 16

_main:
start:
	nop
	cli
	mov	ax,0			; boot sector memory location 0:7c00
	mov	es,ax
	mov	ds,ax			; DS = ES = 0
	mov	ss,ax
	mov	ax,3fffh
	mov	sp,ax
;---- save the boot parameters for conventional read ----
	test	dl,80h	; check for HDD boot
	jnz	_save_boot_parameters

	mov	cl,1	; first sector
	mov	dh,0	; head 0

_save_boot_parameters:
	push	cx		; save cylinder and sector
	push	dx		; save device and head
;--------------------------------------------------------	
	
;---- set video mode 80x25 ----------------------
	;mov	ax,0003h
	;int	10h
;------------------------------------------------	

	mov	ax,0x0e31		; print char function '1'
	int	0x10			; call bios

	;get number of sectors to read
	mov	si,0x7e00
_marker:
	dec	si
	cmp	si,0
	je	__err

	mov	al, [si]	
	cmp	al,0f1h		
	jne	_marker

;---- found the first part of marker	
	mov	al, [si-1]
	cmp	al,0dbh
	jne	_marker
;---- found the seccond part of marker
	pop	dx			; restore device number and head number
					; if extensions allowed for the device,
					; only DL (device number) will be used
	mov	ah,41h
	mov	bx,55aah
	int	13h
	jc	_conventional_read
	cmp	bx,55aah
	jne	_conventional_read
	
	;--- temporary code ---
	; it must be reimplamented later
	mov	ah,0eh	; print char function
	mov	al,'X'	; 'X' --> AL
	int	10h	; call bios
	;-----------------------
	
_conventional_read:
	mov	al,[si+1]	; AL loaded with num of sectors to read
	dec	al		; exclude boot sector
	pop	cx		; restore cylinder and sector
	inc	cl		; skip boot sector
	mov	ah,2		; sector read function
	mov	bx,4000h	; stage 2 start location
	sti
	int	13h		; call bios
	jc	__err

	jmp	0:4000h
	
	
__err:
	mov	ah,0eh		; print char function
	mov	al,'e'		; 'e' --> AL
	int	10h		; call bios
	hlt

absolute _main + 0x200
stage2:

