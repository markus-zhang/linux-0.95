#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern char * strerror(int errno);

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		(C) 1991 Linus Torvalds
 */

#ifdef MARKUS_OUT

extern inline char * strcpy(char * dest,const char *src)
{
__asm__("cld\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	::"S" (src),"D" (dest):"si","di","ax");
return dest;
}

#else

/*
	Observation:
	- Both ESI and EDI are input/output
		- lodsb/stosb needs an initial value from these registers;
		- lodsb/stosb increments these registers in each loop;
		- Both need temp variables;
	- AX is used in lodsb, but no initial value is required, and neither do we need to preserve its value beyond the routine.
		- So it gets into the clobber list to tell GCC don't put anything important in it as it gets updated.
		- Rule of thumb for clobber list: things the code writes into but don't expose as outputs.
	- "cc" and "memory" in clobber list, obviously.
*/
extern inline char * strcpy(char * dest,const char *src)
{
	char *d = dest;
	const char *s = src;
	__asm__(
		/* Clear directional flag (DF), so that future string operations increment ESI/EDI. */
		"cld\n"
		/* Load byte at address DS:(E)SI into AL. Increment (as DF = 0) ESI afterwards. */
		"1:\tlodsb\n\t"
		/* For legacy mode, store AL at address ES:(E)DI. Increment (as DF = 0) EDI afterwards. */
		"stosb\n\t"
		/* Set ZF if al is 0. Note that lodsb loads one byte into AL. This is to check the terminating character '\0'. */
		"testb %%al,%%al\n\t"
		/* Jump to 1: label backwards, if al is not zero. This is how the routine loops. */
		"jne 1b"
		:	"+S" (s),
			"+D" (d)
		:	
		:	"cc", "memory", "eax"
	);

	return dest;
}


#endif

#ifdef MARKUS_OUT

extern inline char * strncpy(char * dest,const char *src,size_t count)
{
__asm__("cld\n"
	"1:\tdecl %2\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"rep\n\t"
	"stosb\n"
	"2:"
	::"S" (src),"D" (dest),"c" (count):"si","di","ax","cx");
return dest;
}

#else

/*
	Observations:
	- "c" is input/output (requires an initial value, and then decremented)
		- input/output: decl %2 both reads an initial value and writes into it;
		- We can use a temp var, but it is not mandatory to keep the "count" copied in this scope.
	- EDI and ESI both input/output
		- We need a temp varaible for dest because the initial value is returned, based on the original semantic.
		- It is OK to NOT use a temp var for src because the function doesn't use it in its scope after the routine.
	- AX is used in the routine but not as an input or output, so it's in the clobber list.
		- However, after consulting ChatGPT, it recommends a new method:
			- Use a temp var for eax, and designate it as output "=" and early clobber "&"
			- https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Modifiers.html#Modifiers
	- "cc" and "memory" definitely clobbered.
*/

extern inline char * strncpy(char * dest,const char *src,size_t count)
{
	char *d = dest;
	const char *s = src;
	unsigned long tmp;

	__asm__(
		/* Clear DF. Future string ops increment EDI/ESI after each execution. */
		"cld\n"
		/* Decrement count */
		"1:\tdecl %2\n\t"
		/* If count is negative (SF set), jump forward to label 2f. */
		"js 2f\n\t"
		/* Load byte at address DS:(E)SI into AL. Increment (as DF = 0) ESI afterwards. */
		"lodsb\n\t"
		/* For legacy mode, store AL at address ES:(E)DI. Increment (as DF = 0) EDI afterwards. */
		"stosb\n\t"
		/* Set ZF if al is 0. Note that lodsb loads one byte into AL. This is to check the terminating character '\0'. */
		"testb %%al,%%al\n\t"
		/* If al is not zero (src not terminated), jump backwards to label 1: for another loop. */
		"jne 1b\n\t"
		/* This is to fill the rest of dest with 0, I think, when src is zero but count is not 0 yet. */
		/* For ecx repetitions, store AL at address ES:(EDI). Increment (as DF = 0) EDI for each repeat. */
		"rep\n\t"
		"stosb\n"
		"2:"
		:	"+S" (s), "+D" (d), "+c" (count), "=&a" (tmp)
		:
		:	"cc", "memory"
	);

	return dest;
}

#endif

#ifdef MARKUS_OUT

extern inline char * strcat(char * dest,const char * src)
{
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	::"S" (src),"D" (dest),"a" (0),"c" (0xffffffff):"si","di","ax","cx");
return dest;
}

#else

/*
	The code basically does:
	for (; *dest != 0; dest++)	{}
	for (; *src != 0; *(dest++) = *(src++))	{}

	Observations:
	- EDI definitely needs a temp var as it get changed in each repeat;
	- ESI doesn't require a temp var but doesn't hurt to have one;
	- Both EDI and ESI are input/output;
	- EAX is input, but is changed in each loop, so should be "+a", even if we don't care about the final value;
	- EAX also needs a var to contain initial value 0x0;
	- ECX is both input and output, and needs a var to contain initial value 0xFFFFFFFF;
*/

extern inline char * strcat(char * dest,const char * src)
{
	unsigned long __c = 0xFFFFFFFF;
	unsigned long __a = 0;
	char *d = dest;
	const char *s = src;
	__asm__(
		/* Clear DF. This makes subsequent string operations increment ESI/EDI. */
		"cld\n\t"
		/* Repeat SCASB until ECX = 0 or ZF = 1; */
		/* https://c9x.me/x86/html/file_module_x86_id_279.html */
		/* Initial value of ECX is set to 0xFFFFFFFF which gives the maximum number of steps of repeats. */
		/* SCASB Compare AL with byte at ES:(E)DI or RDI then set status flags. */
		/* Note that AL's initial value is 0, so this part of the routine seeks the end of the string. */
		/* It says, given a mximum of 0xFFFFFFFF steps, find the '\0' byte starting from EDI. Increment EDI for each loop. */
		"repne\n\t"
		"scasb\n\t"
		/* Decrement EDI. This points EDI back to the '\0', which is where we start the concatenation. */
		"decl %1\n"
		/* Load byte at address DS:(E)SI into AL. Increment (as DF = 0) ESI afterwards. */
		/* This loads one byte of the second string (pointed by an incrementing ESI) into AL. */
		/* ESI initially points to the first byte of src, and increments after each lodsb. */
		"1:\tlodsb\n\t"
		/* For legacy mode, store AL at address ES:(E)DI. Increment (as DF = 0) EDI afterwards. */
		/* This writes AL (containing one byte of the second string) at an incrementing EDI. */
		/* EDI initially points to the terminating character of the first string, and increments after each stosb. */
		"stosb\n\t"
		/* Check if we have reached the terminating char of the second string (AL contains '\0'). */
		"testb %%al,%%al\n\t"
		/* If AL is not zero, jump backwards to first 1: label. */
		"jne 1b"
		:	"+S" (s), "+D" (d), "+c" (__c), "+a" (__a)
		:
		:	"cc", "memory"
	);

	return dest;
}

#endif

#ifdef MARKUS_OUT

extern inline char * strncat(char * dest,const char * src,size_t count)
{
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n\t"
	"movl %4,%3\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %2,%2\n\t"
	"stosb"
	::"S" (src),"D" (dest),"a" (0),"c" (0xffffffff),"g" (count)
	:"si","di","ax","cx");
return dest;
}

#else

/*
	Observation:
	- EDI and ESI are input/output. EDI needs a temp but we will give ESI a temp too, won't hurt.
	- EAX should also have "+" because it is input and changed in each loop.
	- ECX needs a temp for initial value, although it is not actually used in this routine. It is both input/output.
	- "g" means: Any register, memory or immediate integer operand is allowed, except for registers that are not general registers. 
		- Not sure WTH does that mean.
	- "cc" and "memory" in clobber list.
*/

extern inline char * strncat(char * dest,const char * src,size_t count)
{
	unsigned long __c = 0xffffffff;
	unsigned long __a = 0x0;
	char *d = dest;
	const char *s = src;

	__asm__(
		/* Clear DF. Subsequent string operations increment EDI/ESI. */
		"cld\n\t"
		/* repne scasb: Repeat SCASB for ECX times, or when ZF is set.
			- ECX is reduced by 1 for each repeat;
			- SCASB compares 1 byte from AL with (E)DI, then increments EDI by 1 as DF is cleared.
			- If AL is 0 then ZF is automatically set. This is to find the terminating character of char * dest.
		*/
		"repne\n\t"
		"scasb\n\t"
		/* Decrement EDI so it goes back to point at the terminating character. */
		"decl %1\n\t"
		/* Move count into CX. Now we can only cat at most "count" numnber of characters, unlike in strcpy(). */
		"movl %4,%3\n"
		/* Decrement CX */
		"1:\tdecl %3\n\t"
		/* Jump forward to label 2: if CX is negative */
		"js 2f\n\t"
		/* Load one byte from (E)SI to AL, then increment ESI by 1 as DF is cleared. */
		"lodsb\n\t"
		/* Store one byte from AL to (E)DI, then increment EDI by 1 as DF is cleared. */
		"stosb\n\t"
		/* Check if AL is 0. This is to test whether the string has terminated. */
		"testb %%al,%%al\n\t"
		/* If not, go back to 1: */
		"jne 1b\n"
		/* This comes from the ^ js 2f line. If "count" number of characters are concatenated, we are done. */
		/* Set AL to 0 so that stosb can write it as the terminating byte. */
		"2:\txorl %2,%2\n\t"
		"stosb"
		:	"+S" (s), "+D" (d), "+a" (__a),"+c" (__c)
		:	"g" (count)
		:	"cc", "memory"
	);

	return dest;
}

#endif

#ifdef MARKUS_OUT

extern inline int strcmp(const char * cs,const char * ct)
{
register int __res __asm__("ax");
__asm__("cld\n"
	"1:\tlodsb\n\t"
	"scasb\n\t"
	"jne 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"jmp 3f\n"
	"2:\tmovl $1,%%eax\n\t"
	"jl 3f\n\t"
	"negl %%eax\n"
	"3:"
	:"=a" (__res):"D" (cs),"S" (ct):"si","di");
return __res;
}

#else

/*
	Observations:
	- No brainer, turn __res into an ordinary variable;
	- EDI/ESI with "+" and I'll devote a temp variable for each, although none actually needs one.
		- But imagine someone comes to modify the function, and uses cs and ct AFTER the assembly routine, then we must preserve.
	- EAX is OK to be "=" because the routine doesn't ask for its initial value.
	- "cc" and "memory" in clobber list.
*/

extern inline int strcmp(const char * cs,const char * ct)
{
	int __res;
	const char* __cs = cs;
	const char* __ct = ct;
	__asm__(
		/* Clear DF so that subsequent string instructions increment EDI/ESI. */
		"cld\n"
		/* Load one byte from E(SI) to AL. Increment ESI once done, as DF is cleared. */
		"1:\tlodsb\n\t"
		/* Compare AL with E(DI). Increment EDI once done as DF is cleared. Set ZF if they equal. Set SF if  */
		/* lodsb and scasb combined to compare the two strings char by char. */
		"scasb\n\t"
		/* If the two chars are not equal, jump forward to label 2:. */
		"jne 2f\n\t"
		/* Check whether AL is 0. AL is 0 is *ct is '\0'. */
		"testb %%al,%%al\n\t"
		/* Go back to the comparison (label 1:) if *ct is not '\0'. */
		"jne 1b\n\t"
		/* But if *ct is '\0', we are getting to the end. Clear EAX to return 0. */
		"xorl %%eax,%%eax\n\t"
		/* Jump forward to label 3: . */
		"jmp 3f\n"
		/* This is the branch when a comparison failed. Set EAX to 1. */
		"2:\tmovl $1,%%eax\n\t"
		/* strcmp returns 1 if *cs > *ct, -1 if *cs < *ct. Explanation below. */
		/* SCASB is the comparison instruction that sets the flags. JL simply checks the sign flag. */
		/* SCASB compares *ct with *cs. So if *ct < *cs, then the sign flag is set, and JL jumps to 3:. The program returns 1. */
		"jl 3f\n\t"
		/* Negate EAX to return -1 if *cs < *ct. */
		"negl %%eax\n"
		"3:"
		:	"=a" (__res), "+D" (__cs), "+S" (__ct)
		:
		:	"cc","memory");

	return __res;
}

#endif

#ifdef MARKUS_OUT

extern inline int strncmp(const char * cs,const char * ct,size_t count)
{
register int __res __asm__("ax");
__asm__("cld\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"scasb\n\t"
	"jne 3f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %%eax,%%eax\n\t"
	"jmp 4f\n"
	"3:\tmovl $1,%%eax\n\t"
	"jl 4f\n\t"
	"negl %%eax\n"
	"4:"
	:"=a" (__res):"D" (cs),"S" (ct),"c" (count):"si","di","cx");
return __res;
}

#else
/*
	Definition of input/output in GCC inline asm:
	- input: a C value the compiler must make available BEFORE THE ASM STARTS in some location (reg/mem) as the asm will use it;
	- ouput: a C value the compiler should treat as produced AFTER THE ASM ENDS;

	Observations:
	- __res doesn't need to be a register variable;
	- SI register is an input;
		- input/output: lodsb (it reads and changes ESI)
	- DI register is an input/output:
		- input/output: scasb (it reads and changes EDI)
	- AX register is both output:
		- output: movl $1, %%eax
		- No initial value required by the code, so not input
		- No need for a temp var, as it is supposed to be changed and returned, but should be taken off the clobber list.
	- CX register is input/output
		- input/output: decl %3
		- Use a temp var to preserve count, although not necessary.
	- No scratch register
	- Both cc and memory are clobbered
*/
extern inline int strncmp(const char * cs,const char * ct,size_t count)
{
	int __res;
	const char * d = cs;
	const char * s = ct;
	size_t __count = count;
	__asm__(
		/* Clear directional flag */
		"cld\n"
		/* Decrement ECX */
		"1:\tdecl %3\n\t"
		/* JS (Jump if negative) to first 2: label forward */
		"js 2f\n\t"
		/* Load byte at address DS:(E)SI into AL. Also increments ESI by 1 afterwards. */
		"lodsb\n\t"
		/* Compare AL with byte at ES:(E)DI or RDI then set status flags. Also increments EDI by 1 afterwards. */
		"scasb\n\t"
		/* JNE to first 3: label forward (This is the branch when the compare fails) */
		"jne 3f\n\t"
		/* Check whether AL is 0, set ZF if AL is 0 */
		"testb %%al,%%al\n\t"
		/* JNE to first 1: label backward */
		"jne 1b\n"
		/* clear EAX, prepare to return. This is the branch when all comparisons nod. */
		"2:\txorl %%eax,%%eax\n\t"
		/* JMP to first 4: label forward */
		"jmp 4f\n"
		/* Move 1 to EAX */
		"3:\tmovl $1,%%eax\n\t"
		/* JL (jump if less) to first 4: label forward */
		"jl 4f\n\t"
		/* Negate EAX */
		"negl %%eax\n"
		"4:"
		:	"=a" (__res), "+D" (d), "+S" (s), "+c" (__count)
		:	
		:	"cc","memory"
	);
	return __res;
}

#endif

#ifdef MARKUS_OUT

extern inline char * strchr(const char * s,char c)
{
register char * __res __asm__("ax");
__asm__("cld\n\t"
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"je 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"movl $1,%1\n"
	"2:\tmovl %1,%0\n\t"
	"decl %0"
	:"=a" (__res):"S" (s),"0" (c):"si");
return __res;
}

#else

/*
	Observations:
	- (For this item please refer to the original code ^) EAX is a bit special here. It is supposed to contain a pointer to char,
		but is assigned to the value of c before the asm routine runs, by "0" (c). A better design is to split the two into two vars.
		__res should continue serve as the pointer to a char, and I should use another register to be the scratch one.
		BUT, this is too much of change, so I decided to keep the original code as well as the messy "0".
	- "0" just means that AX is initialized with c (NOT that somehow the pointer to char is set to c --that's illegal).
	- ESI is "+" as usual.
	- "cc" and "memory" in clobber list.

*/

extern inline char * strchr(const char * s,char c)
{
	char * __res;

	__asm__(
		/* Clear DF. Subsequent string instructions increment ESI/EDI. */
		"cld\n\t"
		/* Move AL to AH as backup. AX is loaded with c in the beginning. "0" initializes AX (__res) with c. */
		"movb %%al,%%ah\n"
		/* Load byte from (E)SI into AL. Increments ESI afterwards as DF is cleared. */
		"1:\tlodsb\n\t"
		/* Compare AL (*s) with AH (c). Set SF if AL < AH. Set ZF if AH = AL. Basically using the logic of SUB. */
		/* Note that it is DEST - SRC if we want to use the analog of SUB to determine SF. */
		"cmpb %%ah,%%al\n\t"
		/* If equal, we already find the answer. */
		"je 2f\n\t"
		/* Branch of no match: check if AL (*s) is 0. This is to check the terminating char. */
		"testb %%al,%%al\n\t"
		/* Jump backwards to label 1: if ZF = 0. That is, if *s is NOT the terminating char. */
		"jne 1b\n\t"
		/* If we have reached the terminating char, it means we haven't found a matching char */
		/* Move 1 into ESI, so that in the next instruction we can move 1 into __res for return. */
		"movl $1,%1\n"
		/* Branch of matching: move s (address of the char next to the one matching c as lodsb auto-increments) to EAX */
		/* This is to prepare for return. */
		"2:\tmovl %1,%0\n\t"
		/* If found a matching char -> Decrement EAX so that it contains the exact address of the char matching c */
		/* If haven't found a matching char -> return 0, which is NULL */
		"decl %0"
		:	"=a" (__res), "+S" (s)
		:	"0" (c)
		:	"cc", "memory"
	);

	return __res;
}

#endif

#ifdef MARKUS_OUT

extern inline char * strrchr(const char * s,char c)
{
register char * __res __asm__("dx");
__asm__("cld\n\t"
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"jne 2f\n\t"
	"movl %%esi,%0\n\t"
	"decl %0\n"
	"2:\ttestb %%al,%%al\n\t"
	"jne 1b"
	:"=d" (__res):"0" (0),"S" (s),"a" (c):"ax","si");
return __res;
}

#else

/*
	Locate last occurence of character c in string s. Equivalent to the following C snippet:

	char *__res = 0;
	for (; s++; *s != '\0')
	{
		if (*s == c)
			__res = s;
	}
	return __res;

	Observations:
	- ESI should have "+S".
	- "cc" and "memory" in clobber list.
	- "0" and "=d" are correct but messy, leave it.
	- AX should be "+".
	- Just for safety, use a temp for c and s. Imagine someone decides to add code to use c or s after the asm routine.
*/

extern inline char * strrchr(const char * s,char c)
{
	char * __res;
	const char *__s = s;
	unsigned char __c = c;

	__asm__(
		/* Clear DF. Subsequent string instructions increment EDI/ESI. */
		"cld\n\t"
		/* Move AL to AH. This is to duplicate c (as a char, lives in AL) for future comparison. */
		"movb %%al,%%ah\n"
		/* Load byte from (E)SI into AL. Increment ESI afterwards, as DF is cleared. */
		"1:\tlodsb\n\t"
		/* Compare AL with AH. SF logic equals SUB with AL-AH. Set ZF if equal. */
		/* This is to compare c with *s. */
		"cmpb %%ah,%%al\n\t"
		/* If *s != c, jump forward to label 2: */
		"jne 2f\n\t"
		/* If *s == c, move ESI into dx. ESI contains the address of the char next to the one that matches c. */
		/* Because lodsb auto-increments, remember? We need to go back one step. */
		"movl %%esi,%0\n\t"
		/* Go back to the address that actually matches c. */
		/* Since we want to find the LAST char that matches c, we have to keep going. */
		/* But before we go, we need to save ESI into DX and then decrement it by 1, because this could be the last char. */
		"decl %0\n"
		/* Clear AL. */
		"2:\ttestb %%al,%%al\n\t"
		/* Go back */
		"jne 1b"
		:	"=d" (__res), "+S" (__s), "+a" (__c)
		:	"0" (0)
		:	"cc","memory"
	);

	return __res;
}

#endif

#ifdef MARKUS_OUT

extern inline size_t strspn(const char * cs, const char * ct)
{
register char * __res __asm__("si");
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res-cs;
}

#else

/*
	Returns the length of the inital portion of cs which consists only characters that are part of ct ??

	Observations:
	- "g": Any register, memory or immediate integer operand is allowed, except for registers that are not general registers. 
	- Keep "0" and "=S" combo. Messy but technically S is not an input, just initialized with "0" before the routine starts.
	- "a", "c" and "g" are fine. They are input, but not output.
	- "cc" and "memory" in clobber list.
	- EDI is weird. I can't put it into output list with "+" because I can't assign a lvalue to it. 
		So I just keep it in the clobber list.
	- EDX is also used. But there is no initial value required, so not in input list. I put it in clobber list.

	ChatGPT also clarifies a couple of rules which I agree with:
	- Use a clobber when the register is purely internal scratch / implicitly trashed and you don’t need to set it up from C.
	- Use an operand ("c"(x), "+c"(x), "=c"(x)) when you need to initialize it, preserve/return something, 
		or you want the compiler to allocate/register-match it for you.
*/

extern inline size_t strspn(const char * cs, const char * ct)
{
	char * __res;
	size_t tmp = 0xFFFFFFFF;
	unsigned int __a = 0;
	const char* __cs = cs;
	const char* __ct = ct;

	__asm__(
		/* Clear DF. Subsequent string instructions increments ESI/EDI. */
		"cld\n\t"
		/* Move ct into EDI. */
		"movl %4,%%edi\n\t"
		/* Repeat SCASB ECX times until ECX = 0 or ZF = 1, for each repeat decrement ECX by 1. */
		/* Since ECX is initialized as 0xFFFFFFFF, this is just to maximize it. */
		/* SCASB compare AL with byte in (E)DI. Increment EDI afterwards as DF is cleared. */
		/* Since AL is 0, this is to walk to the end of the string until hit the terminating char. */
		"repne\n\t"
		"scasb\n\t"
		/* NOTL + DECL set ECX to its negative value (e.g. -1 turns into 1). */
		/* After these two instructions, ECX contains the length of the string ct. */
		"notl %%ecx\n\t"
		"decl %%ecx\n\t"
		/* Move ECX (len of cs string) into EDX. */
		"movl %%ecx,%%edx\n"
		/* Load byte from E(SI), which is *cs, into AL. Increment ESI afterwards as DF is cleared. */
		"1:\tlodsb\n\t"
		/* Check if AL is 0 */
		"testb %%al,%%al\n\t"
		/* if yes then jump forward to label 2:. */
		"je 2f\n\t"
		/* Move ct to EDI. */
		"movl %4,%%edi\n\t"
		/* Move EDX (len of cs string) back to ECX. */
		"movl %%edx,%%ecx\n\t"
		/* Repeat SCASV ECX times until ECX = 0 or ZF = 1, for each repeat decrement ECX by 1. */
		/* This makes sense as ECX contains len of cs string, so maximum matching len is len of cs. */
		/* SCASB compare AL with byte in (E)DI (*ct). Increment EDI afterwards as DF is cleared. */
		"repne\n\t"
		"scasb\n\t"
		"je 1b\n"
		"2:\tdecl %0"
		:	"=S" (__res), "+a" (__a), "+c" (tmp)
		:	"0" (__cs),"g" (__ct)
		:	"cc", "memory", "di", "dx"
	);

	return __res-cs;
}

#endif

extern inline size_t strcspn(const char * cs, const char * ct)
{
register char * __res __asm__("si");
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res-cs;
}

extern inline char * strpbrk(const char * cs,const char * ct)
{
register char * __res __asm__("si");
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n\t"
	"decl %0\n\t"
	"jmp 3f\n"
	"2:\txorl %0,%0\n"
	"3:"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res;
}

extern inline char * strstr(const char * cs,const char * ct)
{
register char * __res __asm__("ax");
__asm__("cld\n\t" \
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"	/* NOTE! This also sets Z if searchstring='' */
	"movl %%ecx,%%edx\n"
	"1:\tmovl %4,%%edi\n\t"
	"movl %%esi,%%eax\n\t"
	"movl %%edx,%%ecx\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 2f\n\t"		/* also works for empty string, see above */
	"xchgl %%eax,%%esi\n\t"
	"incl %%esi\n\t"
	"cmpb $0,-1(%%eax)\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"2:"
	:"=a" (__res):"0" (0),"c" (0xffffffff),"S" (cs),"g" (ct)
	:"cx","dx","di","si");
return __res;
}

#ifdef MARKUS_OUT

extern inline size_t strlen(const char * s)
{
register int __res __asm__("cx");
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %0\n\t"
	"decl %0"
	:"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):"di");
return __res;
}

#else

extern inline size_t strlen(const char * s)
{
	int __res; /* No need to specify cx, "=c" (__res) picks ecx */
	__asm__(
		/* Clears direction flag, so that pointer increments for SCAS */
		/* https://stackoverflow.com/questions/9636691/what-are-cld-and-std-for-in-x86-assembly-language-what-does-df-do */
		"cld\n\t"
		/* REPNE SCASB: Compare AL, starting at [EDI] (keep changing EDI while [EDI] != AL) */
		/* Whether it increments or decrements EDI depends on the direction flag */
		/* https://www.cs.uaf.edu/2017/fall/cs301/lecture/10_06_string_inst.html */
		/* It also decrement ECX for each loop */
		/* https://stackoverflow.com/questions/26783797/repnz-scas-assembly-instruction-specifics */
		"repne\n\t"
		"scasb\n\t"
		/* 2's complement -- NOT then DEC */
		/* Recall that ^ explains that ECX gets decremented for each loop, but ECX starts with 0xffffffff, which is negative. */
		/* So that's why we revert it back to positive number. */
		"notl %0\n\t"
		"decl %0"
		/* "=c" means ecx is the output, and __res contains the output value */
		/* "D" (EDI) is input, which is s; But scasb also changes it for comparison, so also an output */
		/* "a" (ax) is input, which is 0 initially */
		/* "0" (0xffffffff) basically says, initialize %0, which is ecx to 0xffffffff, -1 in 2's complement */
		:	"=c" (__res),
			"+D" (s)
		:	"a" (0),"0" (0xffffffff)
		:	"cc", "memory"
	);
	return __res;
}

#endif

extern char * ___strtok;

extern inline char * strtok(char * s,const char * ct)
{
register char * __res __asm__("si");
__asm__("testl %1,%1\n\t"
	"jne 1f\n\t"
	"testl %0,%0\n\t"
	"je 8f\n\t"
	"movl %0,%1\n"
	"1:\txorl %0,%0\n\t"
	"movl $-1,%%ecx\n\t"
	"xorl %%eax,%%eax\n\t"
	"cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"je 7f\n\t"			/* empty delimeter-string */
	"movl %%ecx,%%edx\n"
	"2:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 7f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 2b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 7f\n\t"
	"movl %1,%0\n"
	"3:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 5f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 3b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 5f\n\t"
	"movb $0,(%1)\n\t"
	"incl %1\n\t"
	"jmp 6f\n"
	"5:\txorl %1,%1\n"
	"6:\tcmpb $0,(%0)\n\t"
	"jne 7f\n\t"
	"xorl %0,%0\n"
	"7:\ttestl %0,%0\n\t"
	"jne 8f\n\t"
	"movl %0,%1\n"
	"8:"
#if __GNUC__ == 2
	:"=r" (__res)
#else
	:"=b" (__res)
#endif
	,"=S" (___strtok)
	:"0" (___strtok),"1" (s),"g" (ct)
	:"ax","cx","dx","di");
return __res;
}

extern inline void * memcpy(void * dest,const void * src, size_t n)
{
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	:"cx","si","di");
return dest;
}

extern inline void * memmove(void * dest,const void * src, size_t n)
{
if (dest<src)
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	:"cx","si","di");
else
__asm__("std\n\t"
	"rep\n\t"
	"movsb\n\t"
	"cld"
	::"c" (n),"S" (src+n-1),"D" (dest+n-1)
	:"cx","si","di");
return dest;
}

extern inline int memcmp(const void * cs,const void * ct,size_t count)
{
register int __res __asm__("ax");
__asm__("cld\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 1f\n\t"
	"movl $1,%%eax\n\t"
	"jl 1f\n\t"
	"negl %%eax\n"
	"1:"
	:"=a" (__res):"0" (0),"D" (cs),"S" (ct),"c" (count)
	:"si","di","cx");
return __res;
}

extern inline void * memchr(const void * cs,char c,size_t count)
{
register void * __res __asm__("di");
if (!count)
	return NULL;
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1f\n\t"
	"movl $1,%0\n"
	"1:\tdecl %0"
	:"=D" (__res):"a" (c),"D" (cs),"c" (count)
	:"cx");
return __res;
}

#ifdef MARKUS_OUT

extern inline void * memset(void * s,char c,size_t count)
{
__asm__("cld\n\t"
	"rep\n\t"
	"stosb"
	::"a" (c),"D" (s),"c" (count)
	:"cx","di");
return s;
}

#else

extern inline void * memset(void * s,char c,size_t count)
{
	/*
		Judging from the original code:
		- s is not modified -- i.e. the function returns that original value of s
		- c is an ASCII character most likely, so an unsigned char is better
	*/
	void *__temp = s;
	unsigned char ch = (unsigned char)c;

	__asm__(
		/* Clears direction flag, so that pointer increments for SCAS. */
		/* https://stackoverflow.com/questions/9636691/what-are-cld-and-std-for-in-x86-assembly-language-what-does-df-do */
		"cld\n\t"
		/* For ECX repetitions, stores the contents of eax into [EDI], then change EDI, decrement ECX by 1 for each loop, stop when ECX is 0. */
		/* https://stackoverflow.com/questions/3818856/what-does-the-rep-stos-x86-assembly-instruction-sequence-do */
		/* AX is for input, EDI is for input/output, CX is for input/output */
		"rep\n\t"
		"stosb"
		/* Use __temp to preserve s */
		:	"+D" (__temp),
			"+c" (count)
		:	"a" (ch)
		/* No need to clobber "cc" because none of the conditional flags is touched. */
		:	"memory"
	);
	
	return s;
}

#endif

#endif
