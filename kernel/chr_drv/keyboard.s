# 1 "keyboard.S"
# 1 "<built-in>"
# 1 "<command line>"
# 1 "keyboard.S"























.text
.globl _hard_reset_now
.globl _keyboard_interrupt
.globl _kapplic
.globl _kmode
.globl _kleds
.globl _set_leds




size	= 1024		

head = 4
tail = 8
proc_list = 12
buf = 16

_kapplic:	.byte 0
_kmode:	.byte 0		
_kleds:	.byte 2		
e0:	.byte 0






_keyboard_interrupt:
	cld
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	xorl %eax,%eax		
	inb $0x60,%al
	pushl %eax
	inb $0x61,%al
	jmp 1f
1:	jmp 1f
1:	orb $0x80,%al
	jmp 1f
1:	jmp 1f
1:	outb %al,$0x61
	jmp 1f
1:	jmp 1f
1:	andb $0x7F,%al
	outb %al,$0x61
	jmp 1f
1:	jmp 1f
1:	movb $0x20,%al
	outb %al,$0x20
	popl %eax
	movl $1,%ebx
	cmpb $0xE0,%al
	je end_intr
	movl $2,%ebx
	cmpb $0xE1,%al
	je end_intr
	sti
	call key_table(,%eax,4)
	call _do_keyboard_interrupt
	movl $0,%ebx
end_intr:
	movb %bl,e0
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret






put_queue:
	pushl %ecx
	pushl %edx
	movl _table_list,%edx		# read-queue for console
	movl head(%edx),%ecx
1:	movb %al,buf(%edx,%ecx)
	incl %ecx
	andl $size-1,%ecx
	cmpl tail(%edx),%ecx		# buffer full - discard everything
	je 3f
	shrdl $8,%ebx,%eax
	je 2f
	shrl $8,%ebx
	jmp 1b
2:	movl %ecx,head(%edx)
	movl proc_list(%edx),%ecx
	testl %ecx,%ecx
	je 3f
	movl $0,(%ecx)
3:	popl %edx
	popl %ecx
	ret

ctrl:	movb $0x04,%al
	jmp 1f
alt:	movb $0x10,%al
1:	cmpb $0,e0
	je 2f
	addb %al,%al
2:	orb %al,_kmode
	ret
unctrl:	movb $0x04,%al
	jmp 1f
unalt:	movb $0x10,%al
1:	cmpb $0,e0
	je 2f
	addb %al,%al
2:	notb %al
	andb %al,_kmode
	ret

lshift:
	orb $0x01,_kmode
	ret
unlshift:
	andb $0xfe,_kmode
	ret
rshift:
	orb $0x02,_kmode
	ret
unrshift:
	andb $0xfd,_kmode
	ret

old_leds:
	.byte 2

caps:	testb $0x80,_kmode
	jne 1f
	xorb $4,_kleds
	xorb $0x40,_kmode
	orb $0x80,_kmode
_set_leds:
	movb _kleds,%al
	cmpb old_leds,%al
	je 1f
	movb %al,old_leds
	call kb_wait
	movb $0xed,%al		
	outb %al,$0x60
	call kb_wait
	movb _kleds,%al
	outb %al,$0x60
1:	ret
uncaps:	andb $0x7f,_kmode
	ret
scroll:
	testb $0x03,_kmode
	je 1f
	call _show_mem
	jmp 2f
1:	call _show_state
2:	xorb $1,_kleds
	jmp _set_leds
	
num:	cmpb $0x01,_kapplic
	jne notappl
	movw $0x0050,%ax
applkey:
	shll $16,%eax
	movw $0x4f1b,%ax
	xorl %ebx,%ebx
	jmp put_queue
 
notappl:
	xorb $2,_kleds
	jmp _set_leds





cursor:
	subb $0x47,%al
	jb 1f
	cmpb $12,%al
	ja 1f
	jne cur2		
	testb $0x0c,_kmode
	je cur2
	testb $0x30,_kmode
	jne _ctrl_alt_del
cur2:	cmpb $0x01,e0		
	je cur
	testb $0x03,_kmode	
	jne cur
	cmpb $0x01,_kapplic
	jne notcappl
	movb appl_table(%eax),%al
	jmp applkey
notcappl:
	testb $0x02,_kleds	
	je cur
	xorl %ebx,%ebx
	movb num_table(%eax),%al
	jmp put_queue
1:	ret




cur:	movb cur_table(%eax),%al
	cmpb $'9,%al
	ja ok_cur
	movb $'~,%ah
ok_cur:	shll $16,%eax
	movw $0x5b1b,%ax
	xorl %ebx,%ebx
	cmpb $0x01,_kapplic
	jne put_queue
	movb $0x4f,%ah
	jmp put_queue





num_table:
	.ascii "789-456+1230,"

cur_table:
	.ascii "HA5-DGC+YB623"
	
    
# 278 "keyboard.S"
 
appl_table:
	.ascii "wxyStuvlqrspn"




func:
	subb $0x3B,%al
	jb end_func
	cmpb $9,%al
	jbe ok_func
	subb $18,%al
	cmpb $10,%al
	jb end_func
	cmpb $11,%al
	ja end_func
ok_func:
	testb $0x10,_kmode
	jne alt_func
	cmpl $4,%ecx		
	jl end_func
	movl func_table(,%eax,4),%eax
	xorl %ebx,%ebx
	jmp put_queue
alt_func:
	pushl %eax
	call _change_console
	popl %eax
end_func:
	ret




func_table:
	.long 0x415b5b1b,0x425b5b1b,0x435b5b1b,0x445b5b1b
	.long 0x455b5b1b,0x465b5b1b,0x475b5b1b,0x485b5b1b
	.long 0x495b5b1b,0x4a5b5b1b,0x4b5b5b1b,0x4c5b5b1b


key_map:
	.byte 0,27
	.ascii "1234567890+'"
	.byte 127,9
	.ascii "qwertyuiop}"
	.byte 0,13,0
	.ascii "asdfghjkl|{"
	.byte 0,0
	.ascii "'zxcvbnm,.-"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		
	.byte '-,0,0,0,'+	
	.byte 0,0,0,0,0,0,0	
	.byte '<
	.fill 10,1,0

shift_map:
	.byte 0,27
	.ascii "!\"#$%&/()=?`"
	.byte 127,9
	.ascii "QWERTYUIOP]^"
	.byte 13,0
	.ascii "ASDFGHJKL\\["
	.byte 0,0
	.ascii "*ZXCVBNM;:_"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		
	.byte '-,0,0,0,'+	
	.byte 0,0,0,0,0,0,0	
	.byte '>
	.fill 10,1,0

alt_map:
	.byte 0,0
	.ascii "\0@\0$\0\0{[]}\\\0"
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte '~,13,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0,0,0		
	.fill 16,1,0		
	.byte 0,0,0,0,0		
	.byte 0,0,0,0,0,0,0	
	.byte '|
	.fill 10,1,0

# 620 "keyboard.S"




do_self:
	lea alt_map,%ebx
	testb $0x20,_kmode		
	jne 1f
	lea shift_map,%ebx
	testb $0x03,_kmode
	jne 1f
	lea key_map,%ebx
1:	movb (%ebx,%eax),%al
	orb %al,%al
	je none
	testb $0x4c,_kmode		
	je 2f
	cmpb $'a,%al
	jb 2f
	cmpb $'},%al
	ja 2f
	subb $32,%al
2:	testb $0x0c,_kmode		
	je 3f
	cmpb $64,%al
	jb 3f
	cmpb $64+32,%al
	jae 3f
	subb $64,%al
3:	testb $0x10,_kmode		
	je 4f
	orb $0x80,%al
4:	andl $0xff,%eax
	xorl %ebx,%ebx
	call put_queue
none:	ret






slash:	cmpb $1,e0
	jne do_self
	cmpb $1,_kapplic
	jne notmapplic
	movw $'Q,%ax
	jmp applkey
	
notmapplic:
	movl $'/,%eax
	xorl %ebx,%ebx
	jmp put_queue

star:	cmpb $1,_kapplic
	jne do_self
	movw $'R,%ax
	jmp applkey
	
notsapplic:
	movl $'*,%eax
	xorl %ebx,%ebx
	jmp put_queue

enter:	cmpb $1,e0
	jne do_self
	cmpb $1,_kapplic
	jne do_self
	movw $'M,%ax
	jmp applkey
	
minus:	cmpb $1,_kapplic
	jne do_self
	movw $'S,%ax
	jmp applkey
	
plus:	cmpb $1,_kapplic
	jne do_self
	movw $'l,%ax
	jmp applkey
	





key_table:
	.long none,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long enter,ctrl,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,lshift,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,do_self,do_self,do_self	
	.long do_self,slash,rshift,star		
	.long alt,do_self,caps,func		
	.long func,func,func,func		
	.long func,func,func,func		
	.long func,num,scroll,cursor		
	.long cursor,cursor,minus,cursor	
	.long cursor,cursor,plus,cursor		
	.long cursor,cursor,cursor,cursor	
	.long none,none,do_self,func		
	.long func,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,unctrl,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,unlshift,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,unrshift,none		
	.long unalt,none,uncaps,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		
	.long none,none,none,none		




kb_wait:
	pushl %eax
	pushl %ebx
	movl $10000,%ebx
1:	inb $0x64,%al
	testb $0x02,%al
	je 2f
	decl %ebx
	jne 1b
2:	popl %ebx
	popl %eax
	ret

no_idt:
	.long 0,0





_hard_reset_now:
	sti
	movl $100,%ebx
1:	call kb_wait
	movw $0x1234,0x472	
	movb $0xfe,%al		
	outb %al,$0x64
	decl %ebx
	jne 1b
	lidt no_idt		
	jmp _hard_reset_now
