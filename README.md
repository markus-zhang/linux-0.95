### Tools required

Linux 0.95 is a legacy software which modern build tools do not support. E.g. the `Makefile` at root has a GCC option `-fstrength-reduce`, which has been deprecated for a long time. After a bit of search I figured that I should use GCC 4.1. Because of this constraint, I decided to move the toolchain into a docker container, so that the host environment is not bloated with legacy tools. I'm NOT a fluent Linux user so I do not want to beat the boat while driving it, and I recommend that you focus on this one task, too.

DISCLAIMER: I ALSO USED CHATGPT FROM TIME TO TIME TO HELP ME UNDERSTAND BETTER. OCCASIONALLY I ASKED IT TO GENERATE CODE FOR ME (ESPECIALLY ASSEMBLY) BUT OVERALL VERY FEW LINES OF CODE IS AI GENERATED. IT ALSO GENERATES THE TWO DOCKER FILES.

For the purpose of this exercise, the tools are installed within a docker container. The base image is an EoL debian one. `gcc-4.1` is the primary compiler, and `bin86` is for `as86` and `ln86` that `Makefile` uses.

```Dockerfile
FROM debian/eol:etch-slim

# Make apt tolerate EOL archive metadata + expired signatures
RUN printf '%s\n' \
  'Acquire::Check-Valid-Until "false";' \
  'Acquire::AllowInsecureRepositories "true";' \
  'APT::Get::AllowUnauthenticated "true";' \
  'APT::Install-Recommends "0";' \
  'APT::Install-Suggests "0";' \
  > /etc/apt/apt.conf.d/99eol

# Use archive.debian.org only, and mark as trusted to avoid KEYEXPIRED blocking installs
RUN printf '%s\n' \
  'deb [trusted=yes] http://archive.debian.org/debian etch main' \
  'deb [trusted=yes] http://archive.debian.org/debian-security etch/updates main' \
  > /etc/apt/sources.list

RUN apt-get update && apt-get install -y \
    bash make \
    gcc-4.1 g++-4.1 \
    binutils \
    bin86 \
    coreutils findutils grep sed gawk tar gzip bzip2 \
    file \
 && ln -sf /usr/bin/gcc-4.1 /usr/bin/gcc \
 && ln -sf /usr/bin/g++-4.1 /usr/bin/g++ \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work

```

And the docker composer file. The idea is to create a directory structure of `/work` which contains the Linux-0.95 folder for the source code, and `/out` for the compiled image file.

```Dockerfile
services:
  toolchain:
    build: .
    container_name: linux095-toolchain
    working_dir: /work/linux-0.95
    volumes:
      - ./work:/work
      - ./out:/out
    command: ["sh", "-lc", "sleep infinity"]
```

Project directory structure:

```
~/dev/linux95-docker/
                    |---/work/
                    |        |----docker-compose.yml
                    |		 |----Dockerfile
                    |      	 |----/linux-0.95/<where the repo lives>
                    |
                    |---/out/
```

Repo to be cloned: https://github.com/silentcedar/linux-0.95/tree/master


### Compilation process

*OK this is the meat of this document, and I expect this one to be very long, maybe a hundred pages even, because I'm going to document as many error messages I encounter, its explanation and how did I fix it, as possible. So please sit tight and try to follow along if you so wish. I CANNOT guarantee that I document every error message, though, so don't be mad if I miss anything. I do promise that I will document most of the error messages I see during compilation.*

Once all files and directories have been created, run the following command in bash under the `~/dev/linux95-docker/` directory, i.e. the directory that contains the Dockerfile:

```bash
sudo docker compose up -d --build
# Should show a bunch of installs without issue
sudo docker exec -it linux095-toolchain bash
# jump into the shell in Dockerfile
make
```

**Error 0**: 

`make: cpp: Command not found`

This is not a big deal, just run (note that from now on every time I say "run" it means to run commands in the shell in the docker, not your host shell, **unless otherwise mentioned**) `apt-get install -y cpp-4.1` to install `cpp`. `cpp` is NOT something related to C++, but the C preprocessor. I never had the need to install it in modern Ubuntu so I figured that it has been incorporated into `gcc` in a future version.

**Error 1**: 

```
as -c -o boot/head.o boot/head.s
as: unrecognized option `-c'
```

It took me a while to figure it out. I'm pretty sure `as` does NOT have the `-c` option, at least for the more modern versions, but I couldn't find the doc for 2.17, so I asked ChatGPT what the hell is going on here. ChatGPT replied that I need to replace `as` with `gcc` because `-c` is a `gcc` option. It also recommended another option for `ld`, so eventually I started to run this command instead of `make` because I was reluctant and did not want to modify the Makefile. Both options guarantee that they target 32-bit architecture.

```bash
make AS='gcc -m32' LD='ld -m elf_i386'
```

**Error 2**:

```
boot/head.s:264: Error: alignment not a power of 2
```

OK so let's see what is this about. The code is:

```asm
.align 2
.word 0
gdt_descr:
	.word 256*8-1		# so does gdt (not that that's any
	.long _gdt		# magic number, but it works for me :^)

	.align 3 # line 264, where the error is
_idt:	.fill 256,8,0		# idt is uninitialized

_gdt:	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00c09a0000000fff	/* 16Mb */
	.quad 0x00c0920000000fff	/* 16Mb */
	.quad 0x0000000000000000	/* TEMPORARY - don't use */
	.fill 252,8,0			/* space for LDT's and TSS's etc */
```

I Googled a bit and eventually found a topic on OSDEV forum: https://f.osdev.org/viewtopic.php?t=33128 . The good men on OSDEV said, I shall quote, that "When using elf format, the default for gas nowadays, the argument to .align must be a power of 2. The behaviour is different when using a.out format (which that version of Linux would have used) where the argument specifies the power of 2. So .align 3 (a.out) is equivalent to .align 8 (elf).". OK, so I just changed the code to `.align 8` and, eh, hope for the best -- I mean, I won't know whether it works until it runs, which could be days or weeks after.

**Error 3**:

```
init/main.c:23: error: static declaration of 'fork' follows non-static declaration
init/main.c:24: error: static declaration of 'pause' follows non-static declaration
include/unistd.h:246: error: previous declaration of 'pause' was here
init/main.c:26: error: static declaration of 'sync' follows non-static declaration
include/unistd.h:257: error: previous declaration of 'sync' was here
In file included from include/linux/mm.h:7,
                 from include/linux/sched.h:36,
                 from init/main.c:29:

(omitted warnings)

init/main.c:184: error: static declaration of 'printf' follows non-static declaration
make: *** [init/main.o] Error 1

```

This looks like some function redefinition issue. I went into `main.c` and here are the offending lines:

```C
// Near top of the file.
// _syscall0 and _syscall1 are macros, which basically expand to, taking fork as an example, something close to:
// static inline int fork() { ... }
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall1(int,setup,void *,BIOS)
static inline _syscall0(int,sync)

// printf() is defined a bit further down
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}
```

The errors are straightforward -- OK not so straightforward for me before I got it, because junior C programmers like me don't write code like this so we never saw this kind of errors. These functions are already defined in a header file called `unistd.h`, as indicated in the error messages, and those are non-static functions. So the error message "static declaration follows non-static declaration" is head-on. The fix is NOT trivial, though, because I could not simply change the names of these static functions -- if I say changed `fork` to `fork1`, I'd need to create a non-static version somewhere so that `_syscall0()` could use it to generate the static version, which meant I was back in square 1. I consulted ChatGPT and it taught me a trick -- use #define to rename them back to their original names but use the new name for `_syscallx()`. For `printf()` I simply renamed it to `_printf()` because `main.c` continas the full definition of the static function. I also switched all `printf()` calls to `_printf()` in `main.c`.
 
```C
#define __NR_kfork  __NR_fork
#define __NR_kpause __NR_pause
#define __NR_ksetup __NR_setup
#define __NR_ksync  __NR_sync

static inline _syscall0(int,kfork)
static inline _syscall0(int,kpause)
static inline _syscall1(int,ksetup,void *,BIOS)
static inline _syscall0(int,ksync)

// Somewhere further in the file
static int _printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}
```


**Error 4**:

```
init/main.c: In function 'start_kernel':
init/main.c:180: error: can't find a register in class 'AREG' while reloading 'asm'
make: *** [init/main.o] Error 1
```

The offending code is gcc inline assembly, which is the ugliest thing I have ever seen in this side of the world:

```C
	for(;;)
		__asm__("int $0x80"::"a" (__NR_pause):"ax")
```

I know exactly nothing about inline assembly but I could tell what it does -- it is making a syscall because `__NR_pause` is sort of the number for syscall `pause`. However, I have absolutely no idea what the other stuffs mean, so I consulted the 4.1 manual: https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/index.html#Top . TBH I read section 5.34 and 5.35 a couple of times but couldn't get it 100%, so I asked ChatGPT to give me a detailed explanation. As far as I understand, the semicolons are for outputs, inputs and something called clobber list. If there is nothing follows a semicolon, it means that section is empty. In our example, the output is empty, so there is no output, which makes sense because syscalls don't "output". The whole thing basically says: invoke interrupt 0x80 to syscall pause(). Input is "a" and also put "ax" into the clobber list. Since register "a" is ax, so this means, input register is also in the clobber list, which is what the error message trying to say, but choses not to say, just to confuse people. I have no idea how did Linus manage to compile the whole thing. Maybe the rule was less strict back in the day. I think he used gcc 2.1 to compile 0.96 so this could be an even earlier version of gcc. Here is the fix, which removes "ax" from the clobber list, and tells gcc that condition codes and memory are in the clobber list.

```C
	for(;;)
	{
		// __asm__("int $0x80"::"a" (__NR_pause):"ax");
		// Markus: Previous version is confusing -- it puts syscall No. into ax ("a") but puts ax into the clobber list too.
		// New version: clobber list is "cc", which tells gcc the asm may change the CPU condition codes (EFLAGS bits),
		// as well as "memory", which acts as a compiler barrier for memory. This disable reordering by gcc.
		__asm__ volatile ("int $0x80" :: "a" (__NR_pause) : "cc", "memory");
	}
```


**Error 5**:

```
/tmp/cc1pWDV0.s: Assembler messages:
/tmp/cc1pWDV0.s:447: Error: suffix or operands invalid for `push'
/tmp/cc1pWDV0.s:448: Error: suffix or operands invalid for `push'
/tmp/cc1pWDV0.s:449: Error: suffix or operands invalid for `pushf'
/tmp/cc1pWDV0.s:450: Error: suffix or operands invalid for `push'
/tmp/cc1pWDV0.s:451: Error: suffix or operands invalid for `push'

```

I had no diea WTF this is about. This is apparently some intermediate assembly code. I Googled a bit and found someone said that this was caused by using 64-bit tool to compile 32-bit code. I'm not so sure gcc is in 64-bit mode, but I did add `-m32` in `Makefile`:

```Makefile
CFLAGS	=-Wall -O -fstrength-reduce -fomit-frame-pointer -fno-builtin -std=gnu89 -nostdinc -m32 -I$(CURDIR)/include
```

**Note that you need to add these options to ALL Makefiles, not just the one under the root directory.**

I still don't understand the why, but somehow it fixed the error. In general I'm not comfortable about doing something I don't fully understand, but Linux kernel is such a huge project that I'd spend years chasing every bit of detail, so I'll live with it. With the constraints I face, it will probably take me a couple of decades to be truly good at this kind of things. Darn I wish I started this project 10 years ago, before my son was born, when I was still free.


**Error 6**

Error 6 is essentially the same as Error 5 but proved to be much harder to understand and fix.

```
sched.c: In function 'schedule':
sched.c:162: error: can't find a register in class 'CREG' while reloading 'asm'
make[1]: *** [sched.o] Error 1
```

The inline assembly is apparent cursed. I wonder how many times it took Linus to get it in proper shape. I think a few times at least.

```C
// line 162 is actaully a macro (Why do kernel people so love macros?)
switch_to(next);

// The macro is defined as:
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,_current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,_current\n\t" \
	"ljmp %0\n\t" \
	"cmpl %%ecx,_last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n]) \
	:"cx"); \
}
```

I probably learned more about inline assembly AND C macro in this snippet than reading a book about C, because they don't summon demons in books. First, I hate AT&T, but I can read and get the idea: first, compare `ecx` with `_current`, if equal, jump to the next `1:` label. Then move the value in `dx` to `%1` -- if I have done some bash scripting I could probably guess what those `%1`, `%0` mean, but I haven't done much, oh well. So basically I had a few questions after re-reading the code:

- What does `%1` and `%0` mean?
- What does the `1:` mean?
- What is `_current`? What is `_last_task_used_math`?
- How do I fix the compilation error?

I managed to figure out *some* of the answers are:

- `%0` is the first "thing" in the output/input/clobber list, so in out case, `%0` is `__tmp.a` and `%1` is `__tmp.b`. I later found out from ChatGPT that if there is something in the output list (in our case there is none), I need to push back the numbers. What a cursed language!

- Still don't know what `1:` mean. Decided not to ask ChatGPT. Maybe it's fine to not know 100% ATM so that I have something to do after this project.

- `_current` is mentioned in another asm code: `kernel/sys_call.s:119:2:	movl _current,%eax`, but I never found out another reference of `_last_task_used_math`. BTW I learned a bit about `grep`: 

	`grep -RIn --include='*.c' --include='*.h' --include '*.s' '_current'` -> Find recursively in and under CWD for `_current`, but only look into `.c`, `.h` and `.s` files.

To fix the error, I need to move `cx` out of the clobber list, because it is also in the input list. I did not remove the original code because I wanted to leave my comments intact, so I used an undefined to mask the original code:

```C
#ifdef MARKUS_OUT

#define switch_to(n) {\
	struct {long a,b;} __tmp; \
	/* Markus: Use MARKUS_OUT to comment out code, but without turning them into comments, to increase readability. */ \
	/* Markus: Explanation of the AT&T assembly code: https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Extended-Asm.html */ \
	/* cmpl %%ecx, _current -> cmpl means cmp long in AT&T, so basically comparing ecx with _current. */ \
	/* _current is defined in kernel/sys_call.s */ \
__asm__("cmpl %%ecx,_current\n\t" \
	/* FIXME: je 1f -> jump to 0x1f offset (offset to what base addr ??) */ \
	"je 1f\n\t" \
	/* movw %%dx,%1 -> movw is mov word (2 bytes in x86), %1 is the second input, which is mem addr of __tmp.b (See input section) */ \
	/* So this effectively initiate __tmp.b with dx, which is _TSS(n) */ \
	/* https://stackoverflow.com/questions/33783692/what-does-the-ljmp-instruction-do-in-the-linux-kernel-fork-system-call */ \
	"movw %%dx,%1\n\t" \
	/* xchgl %%ecx,_current -> exchange value of ecx and _current. */ \
	"xchgl %%ecx,_current\n\t" \
	/* ljmp %0 -> ljmp to %0, the first input, which is __tmp.a (See input section) */ \
	"ljmp %0\n\t" \
	/* cmpl %%ecx,_last_task_used_math -> compare ecx with _last_task_used_math */ \
	"cmpl %%ecx,_last_task_used_math\n\t" \
	/* jump to 0x1f offset if not equal */ \
	"jne 1f\n\t" \
	/* CLTS clear the task-switched flag */ \
	"clts\n" \
	"1:" \
	/* Output follows the first :, and Input follows the second : */ \
	/* This piece of code does NOT have output as it shows. */ \
	/* "m" probably means a memory address (to be verified with the manual) */ \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	/* "d" means the d register constraint, and "c" means the c register constraint */ \
	/* https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Machine-Constraints.html#Machine-Constraints and look for i386. */ \
	/* In particular, note that cx is an input register. */ \
	"d" (_TSS(n)),"c" ((long) task[n]) \
	/* Clobber list follows the last :, so this means cx is a clobber. */ \
	/* This contradicts with the above line where cx is also an input register, thus gcc 4.1 complains. */ \
	/* Quote from manual: "You may not write a clobber description in a way that overlaps with an input or output operand." */ \
	:"cx"); \
}

#else

#define switch_to(n) { \
	long __ecx = (long) task[n]; \
	struct {long a,b;} __tmp; \
	__asm__ ( "cmpl %%ecx,_current\n\t" \
			"je 1f\n\t" \
			/* Now that we added an output section, should be %2 and %1 instead of %1 and %0 */ \
			"movw %%dx,%2\n\t" \
			"xchgl %%ecx,_current\n\t" \
			"ljmp %1\n\t" \
			"cmpl %%ecx,_last_task_used_math\n\t" \
			"jne 1f\n\t" \
			"clts\n" \
			"1:" \
			/* "+" means this operand is both read and written by the instruction. Do NOT put it into the input section. */ \
			/* https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Modifiers.html#Modifiers */ \
			: "+c" (__ecx) \
			: "m" (*&__tmp.a), "m" (*&__tmp.b), "d" (_TSS(n)) \
			: "cc", "memory"); \
}
```

I learned a lot from fixing 

- You can't have empty lines in C macros, or I think more precisely, you can have but it needs to end with a `\`.

- EVERY line in a multiple line C macro needs to end with a `\`, not just the one with code. That includes the comments, too.

- You CANNOT use `//` comments in multiple line macros. Why? Because every line needs to end with `\`, which technically means all those lines are stitched together. So if you put a `//` comment, everything afterwards get commented out. You MUST use `/*...*/` to encapsulate the comments.

- You can put something in the output list, give it a `"+c"` prefix, and the manual says it means the operand is both read and written. Once you do it, there is no need to put the same thing into the input list.

I wonder if it is possible to turn this into a proper function, but probably not unless I use intrinsic? Something new to learn, I guess.


**Error 7**:

```
sched.c: In function 'sched_init':
sched.c:454: error: can't find a register in class 'AREG' while reloading 'asm'
sched.c:456: error: can't find a register in class 'AREG' while reloading 'asm'
make[1]: *** [sched.o] Error 1
```

Same shit, different flavors. Showing line 454-456 in `sched.c` and we know the macros hit again:

```C
	// line 454
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	// line 456
	set_system_gate(0x80,&system_call);
```

For some reason VSCode+clangd failed to jump to the definition of these two macros so I had to use `grep`. The definition is at line 33, `include/asm/system.h`:

```
markus@t470s:~/dev/linux095-docker/work/linux-0.95$ grep -RIn 'set_intr_gate'
kernel/blk_drv/floppy.c:528:	set_intr_gate(0x26,&floppy_interrupt);
kernel/sched.c:454:	set_intr_gate(0x20,&timer_interrupt);
kernel/chr_drv/serial.c:92:	set_intr_gate(0x24,rs1_interrupt);
kernel/chr_drv/serial.c:93:	set_intr_gate(0x23,rs2_interrupt);
include/asm/system.h:33:#define set_intr_gate(n,addr) \
```

```C
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

// All three ^ use the same macto _set_gate
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	:: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"m" (*((char *) (gate_addr))), \
	"m" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000) \
	:"ax","dx")
```

We can probably already see what is the issue. Both `ax` and `dx` are in the clobber. The fixes are similar -- we put "cc" and "memory" into the clobber list; Move the two "m" from input list into output list because they are written into only in the assembly 

```C
#define _set_gate(gate_addr,type,dpl,addr) do {					\
	unsigned long __d = (unsigned long)(addr);					\
	unsigned long __a = 0x00080000;								\
	__asm__ (													\
		/* move a word (2-byte) from dx to ax */				\
		"movw %%dx,%%ax\n\t" 									\
		/* move a word from %4 (i) to dx */						\
		"movw %4,%%dx\n\t" 										\
		/* move a long (4-byte) from eax to %1 (1st m) */		\
		/* double % for a literal %  */							\
		"movl %%eax,%0\n\t" 									\
		/* move a long from edx to %2 (2nd m) */				\
		"movl %%edx,%1" 										\
		/* Move two "m"s into output as they are written */ 	\
		/* "=" means write-only operation */					\
		/* Check gcc-4.1.0 online doc 5.35.3*/					\
		: 	"=m" (*((unsigned long *) (gate_addr))),			\
			"=m" (*(4+(char *) (gate_addr))),					\
			/* "+" means both input/output */
			"+d" (__d),											\
			"+a" (__a)			 								\
		: "i" ((short) (0x8000+(dpl<<13)+(type<<8)))  			\
		:"cc","memory");										\
} while (0)
```


**Error 8**:

Same error message but in a different macro. 

```
fork.c: In function 'copy_mem':
fork.c:58: error: can't find a register in class 'DREG' while reloading 'asm'
fork.c:59: error: can't find a register in class 'DREG' while reloading 'asm'
make[1]: *** [fork.o] Error 1
```

And this is just a macro `set_base()` which is just a wrapper for another macro `_set_base()` in `sched.h`. You can see that it is the same issue: `dx` in both the clobber list and the input list.

```C
// Original code
#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2" \
	::"m" (*((addr)+2)), \
	  "m" (*((addr)+4)), \
	  "m" (*((addr)+7)), \
	  "d" (base) \
	:"dx")
```

Now I'm trying to do this by my own. `dx` is part of input, that's for sure, because `movw %%dx, %%0` in AT&T syntax put the value in `dx` into `%0`. And it is also an output, because the next line `rorl $16 %%edx` rotates right 16 of the value stored in `edx` and puts the result back into `edx`. Also, apparently all 3 "m"s are ONLY outputs (they get written into). I think a reasonable replacement is:

```C
#define _set_base(addr,base)	\
__asm__("movw %%dx,%0\n\t" 		\
	"rorl $16,%%edx\n\t" 		\
	"movb %%dl,%1\n\t" 			\
	"movb %%dh,%2" 				\
	:	"=m" (*((addr)+2)), 	\
		"=m" (*((addr)+4)), 	\
		"=m" (*((addr)+7)), 	\
		"+d" (base)				\
	::	"cc", "memory")
```

OK I'll also add the `do...while(0)` loop, so eventually this becomes:

```C
#define _set_base(addr,base) 					\
do {																	\
__asm__(															\
	"movw %%dx,%0\n\t" 									\
	"rorl $16,%%edx\n\t"	 							\
	"movb %%dl,%1\n\t" 									\
	"movb %%dh,%2" 											\
	/* All 3 "m"s are only outputs */		\
	/* But dx is read/write */					\
	:	"=m" (*((addr)+2)), 							\
	  "=m" (*((addr)+4)), 							\
	  "=m" (*((addr)+7)), 							\
	  "+d" (base) 											\
	::"cc", "memory")										\
} while (0)

```

However, we are not done yet! I just found out through ChatGPT that the above code has a flaw. Can you guess what it is? It has something to do with semantics. Let's review the original semantic. In the original code, `dx` is in the input list and the clobber list, **but it is not in the output list**. This means that `base` is NOT to be modified (written out). However, in the new version, since `d` is in both input/output list, this means `base` will be written back (in this case, rotate to the left for 16 bits).

So what I gathered is that in GCC inline assembly, the input/output part follows the variable, not the register. The register is simply used to contain the value of the variable, and whether the value of the variable changes depends on whether it is in the output list, and ofc whether the code actually writes back. Here is a minimum example:

```C
#include <stdio.h>

int main()
{
    #define _test(var, addr) do {    \
        __asm__ volatile (      \
            "movw %%dx, %0"     \
            "addw $0x01, %%dx"  \
            :   "=m" (*(addr)),   \
                "+d" (var)      \
            :                   \
            :   "cc", "memory"  \
        );                       \
    } while (0)
        
    int addr = 0;
    int var = 10;
    _test(var, &addr);
    printf("var is %d\n", var);
    printf("addr is %d\n", addr);

    return 0;
}

```

If you try this example on Godbolt compiler explorer, it outputs nothing for GCC 4.1, probably because of some glitch. But if you raise the version to 5.1, it prints 11 for `var` and 10 for `addr`. This means that while in the assembly code, it *looks* like only `dx` is incremented, but from the perspective of GCC, **the user wants to increment `var`**. This is a very important concept that I need to keep in mind, as GCC inline assembly is NOT exactly assembly but has its own rules. This is really cursed...**I shall reiterate -- in GCC inline assembly, registers are real, hardware registers. However, once you put variables into the output list (like the `var` in `"+d" (var)`), GCC treats the new value as the value of the variable, thuys it modifies its value.**

The solution is to copy `base` to a temp variable, and use that temp variable instead, so that the original variable is not modified. But be careful, don't do this if the semantics is supposed to modify the value of the original variable! Here is the fixed code. I used `unsigned long` because that's what I see in `fork.c` that calls `set_base()`.

```C
#define _set_base(addr,base) 					\
unsigned long __temp = base;					\
do {																	\
__asm__(															\
	"movw %%dx,%0\n\t" 									\
	"rorl $16,%%edx\n\t"	 							\
	"movb %%dl,%1\n\t" 									\
	"movb %%dh,%2" 											\
	/* All 3 "m"s are only outputs */		\
	/* But dx is read/write */					\
	:	"=m" (*((addr)+2)), 							\
	  "=m" (*((addr)+4)), 							\
	  "=m" (*((addr)+7)), 							\
	  "+d" (base) 											\
	::"cc", "memory")										\
} while (0)

````


**Error 9**: (Not exactly an error we see in the compiler log)

While we are at Error 8, might as well do the same thing for the neighboring macro `_set_limit()` too as it has the same issue. We can see that `dx` is used as both input and output; `%1` is used as an input and an output (you can see it's on both sides). `%0` is used as an output.

```C
#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1" \
	::"m" (*(addr)), \
	  "m" (*((addr)+6)), \
	  "d" (limit) \
	:"dx")
```

I need to be really really careful about the analysis. Since I have worked on a few of these inline assemblies, I figured out the parts I need to pay attention to:

- Is it input, or output, or both?
- Is it a memory address, or a register (variable)?
- What is the size of the thing? Look at the asm code to figure it out.

So in this case, we have three items, `%0` and `%1` are memory addresses, and `dx` is a register containing `limit`. We can immediately tell from the code, that `%0` is an input, while `%1` is both an input and an output, and `dx` is both an input and output. I can also tell that for memory addresses, it is OK to get written into, but it is not OK to write back to a variable such as `limit`. Finally I check the sizes -- `%0` is written into by `movw`, so it must be a 2-byte word. `%1` is written into by `movb`, so it is a 1-byte char. I also see `edx` in the code, so this must be a `long`, and looking at how `_set_limit()` is used, I can tell that it should be an `unsigned long`. Adn don't forget about the `do...while(0)` part. So the final result is:

```C
/*
	Similar to above, we need to be very careful about variables that go into BOTH output and input lists.
	If the semantic does not alter the variable, but we need to put it into both input and output lists, use a temp variable.
	We should also identify which variables should go into which list.
	Analysis: (use the ^ original code for reference as the new code is to be changed)
	- edx is on both sides, apparently, so we need a temp variable for limit. It is 4-byte so I chose unsigned long;
	- %0 is output only. %1 is both input/output, but it is a memory address, not a reg, so no need for temp var;
	- %0 is written into by movw, so it is 2-byte. %1 is read and written into dh by movb, so it is 1-byte.
*/
#define _set_limit(addr,limit) 										\
do { 																							\
	unsigned long __limit = (unsigned long) (limit); 	\
	__asm__( 																				\
		"movw %%dx,%0\n\t" 														\
		"rorl $16,%%edx\n\t" 													\
		"movb %1,%%dh\n\t" 														\
		"andb $0xf0,%%dh\n\t" 												\
		"orb %%dh,%%dl\n\t" 													\
		"movb %%dl,%1" 																\
		:	"=m" (*(unsigned short *)(addr)), 					\
			"+m" (*(unsigned char *)((addr)+6)), 				\
			"+d" (__limit) 															\
		:																							\
		:	"cc", "memory"															\
	); 																							\
} while (0)
```


**Error 10**:

Same thing, but with a different flavor. And this piece of assembly code is more interesting than the previous ones.

```
vsprintf.c: In function 'vsprintf':
../include/string.h:266: error: can't find a register in class 'DIREG' while reloading 'asm'
make[1]: *** [vsprintf.o] Error 1

```

The code is in `string.h`, which is `strlen()`:

```C
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
```

I Googled and ChatGPTed extensively to figure out how `strlen()` is implemented, because it uses string operations which I have never seen. I put my observations in comments. Please note that I also fixed the error message.

```C
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
```


**Error 11**:

The next one is similar. It is the `memset()` function in `string.h`. I'll just post the original and the new version. The comments should be good enough to explain the whys. Note that I use macro to disable the original version. So basically the original is on top and the new one is on bottom.

```C
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
```


**Error 12**:

This is a nasty one because it doesn't have a good format. It is the `copy_page()` macro in `memory.c`. I really hate it when it is a macro instead of a function.

```C
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):"cx","di","si")
```

The new version as below. The comments should be detailed enough for self-explanation:

```C
/*
	https://stackoverflow.com/questions/27804852/assembly-rep-movs-mechanism
	Basically rep ALWAYS repeats the following string operation for ECX times, and reduces ECX for each repeat.
	So this piece of code says:
	- First clear direction flag, so that pointer increments for the string operation MOVSL.
	- Then, repeat MOVSL for ECX times, and reduce ECX for each repeat. Stop when ECX is 0.
		- In each repeat, MOVSL copies data from [ESI] to [EDI] and increments (because DF is cleared by CLD) them.

	Conclusion:
	- ECX is input/output, because the program needs to read from/write into it;
	- Both ESI and EDI are input/output, because the program needs to read from them (addressing) and write into them (increment);
	- Semantically, neither from nor to is modified in the program;
*/
#define copy_page(from,to) \
	do {	\
		/* Use temp variables to preserve the semantic */	\
		unsigned long __from = (unsigned long)from;	\
		unsigned long __to = (unsigned long)to;	\
		/* Must use a modifiable lvalue for "+c" */	\
		size_t n = 1024;	\
		__asm__(	\
			"cld ; rep ; movsl"	\
			:	"+S" (from), "+D" (to), "+c" (n)	\
			:	\
			:	"memory"	\
		) \
	} while (0)

```


**Error 13**:

Don't forget to add `-std=gnu89 -nostdinc -m32` into each `Makefile`. I found out that the `Makefile` under `/mm` is missing these lines so got a bunch of weird errors for intermediate assembly files.

```Makefile
CFLAGS	=-O -Wall -fstrength-reduce -fomit-frame-pointer \
	-finline-functions -nostdinc -std=gnu89 -m32 -I../include
```


**Error 14**:

In `swap.c`, `get_free_page()` has a long inline assembly function:

```C
	__asm__("std ; repne ; scasb\n\t"
		"jne 1f\n\t"
		"movb $1,1(%%edi)\n\t"
		"sall $12,%%ecx\n\t"
		"addl %2,%%ecx\n\t"
		"movl %%ecx,%%edx\n\t"
		"movl $1024,%%ecx\n\t"
		"leal 4092(%%edx),%%edi\n\t"
		"rep ; stosl\n\t"
		"movl %%edx,%%eax\n"
		"1:\tcld"
		:"=a" (result)
		:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
		"D" (mem_map+PAGING_PAGES-1)
		:"di","cx","dx");
```

I think I'm getting a hang of this cursed inline assembly. I still use ChatGPT to confirm my findings but for this time the only thing I missed is that I forgot to put `edx` into the clobber list -- just one more thing in the checklist.

```C
	unsigned long __paging_pages = PAGING_PAGES;
	unsigned long __edi = mem_map+PAGING_PAGES-1;

	/*
		AX: 						output only;
		"0" (0): 				initialize AX to 0;
		"i" (LOW_MEM): 	input only;
		CX: 						Both input and output, need a temp variable;
		EDI: 						Both input and output
		EDX:						Scratch register
	*/
	__asm__(
		/* std: set directional flag, so that pointer decrements for SCASB */
		/* repne scasb: https://stackoverflow.com/questions/26783797/repnz-scas-assembly-instruction-specifics */
		/* basically repeat for ECX times: each repeat, compare AL with [EDI], then decrement EDI and ECX */
		"std ; repne ; scasb\n\t"
		/* Jump if not equal, to the next 1. "f" here means going forward. */
		"jne 1f\n\t"
		/* move 1 into [EDI+1], just one byte */
		"movb $1,1(%%edi)\n\t"
		/* shift left ecx (32-bit) by 12 bits */
		/* this is to align the address to 2^12 = 4096 bytes (4KiB) boundaries */
		"sall $12,%%ecx\n\t"
		/* add "i" (LOW_MEM) to ecx */
		"addl %4,%%ecx\n\t"
		/* move ecx to edx */
		"movl %%ecx,%%edx\n\t"
		/* move 1024 into ecx */
		"movl $1024,%%ecx\n\t"
		/* move edx + 4092 into edi */
		"leal 4092(%%edx),%%edi\n\t"
		/* store the content of eax into [EDI], repeat for ECX times */ 
		/* for each repeat reduce ECX by 1, and decrement EDI by 1, so that's 1024 repeats because ECX is 1024 */
		/* I think this is to fill the page with garbage. But I don't know what EAX contains exactly... */
		"rep ; stosl\n\t"
		/* move edx to eax */
		"movl %%edx,%%eax\n"
		"1:\tcld"
		:	"=a" (result),
			"+c" (__paging_pages),
			"+D" (__edi)
		/* https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Simple-Constraints.html#Simple-Constraints */
		/* "i": An immediate integer operand. */
		:	"0" (0),"i" (LOW_MEM)
		/* edx is just a scratch register, not connected to anything, so it goes into the clobber list. */
		:	"cc", "memory", "edx"
	);
```


**Error 15**:

In `string.h`, the function `strncmp()`. I also learned a bit more about what is considered as an "input" constraint in GCC inline assembly. My original understanding was wrong but it happened that it did not impact the code I modified. Basically, **if the assembly code needs an initial value for a C value, then it is part of the input list.**

```C
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
```

Take `ax (__res)` as an example. In the code, `ax` is indeed read (both `testb` and `xorl` read `ax`), but there is NO requirement to have an initial value in `ax`. Think about the equivalent C code. You don't have to initialize `__res` to make this piece of code work, because it is going to be `0` or `-1` based on the result of the comparison. Thus `=a` is good enough.

```C
/*
	Definition of input/output/clobber in GCC inline asm:
	- input: a C value the compiler must make available BEFORE THE ASM STARTS in some location (reg/mem) as the asm will use it;
	- ouput: a C value the compiler should treat as produced AFTER THE ASM ENDS;
	- clobber: things the routine writes into, but don't expose as outputs;

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
```


**Error 16**:

Again, this is not exactly an error, but I decided to proactively transform all inline assembly code left in `string.h` to a modern format -- i.e. can be compiled by GCC 4.1. I'll start with `strcpy()`.

```C
// strcpy()
// I can imagine that the C code looks like this:
/*
char *strcpy(char *dest, const char *src)
{
	for (; dest++, src++; dest != 0)
	{
		*src = *dest;
	}
}
*/

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
```

**Error 17**:

I then changed everything in `string.h`. There is a lot of change so I'll list the most interesting ones here.

First, don't forget to check for edge case. Sometimes a string comparison algorithm compares string a with string b, for n bytes. I need to make sure that edge cases such as `n = 0` and `cs = ct` are taken care of. `memmove()` is such an example.

```C
extern inline void * memmove(void * dest,const void * src, size_t n)
{
	char *d = (char *)dest;
	char *__d = ((char *)dest) + n - 1;
	const char *s = (const char *)src;
	const char *__s = ((const char *)src) + n - 1;

	if (dest == src | n == 0)
	{
		return dest;
	}

	if (dest<src)
		__asm__(
			"cld\n\t"
			"rep\n\t"
			"movsb"
			:	"+c" (n), "+S" (s), "+D" (d)
			:
			:	"cc", "memory"
		);
	else
		__asm__(
			"std\n\t"
			"rep\n\t"
			"movsb\n\t"
			"cld"
			:	"+c" (n), "+S" (__s), "+D" (__d)
			:
			:	"cc", "memory"
		);

	return dest;
}
```

Also, to clarify the general rules again (just in case this is the first routine you read about):
- If the asm routine requires a reg to be initialized, then it's an input.
- If the reg is written into by the asm routine, then it's an output.
- If the reg is both an input/output, it MUST use "+". (Exception: "=S" with "0" is a special case that is allowed. )
	- Damn it this is so cursed.
- If the reg is ONLY output, like a scratch register, and is not linked to a variable, then it can stay in the clobber list.
	- It cannot use "=" because it doesn't have a linked var, so nothing in ().
- If the reg is ONLY output, but linked to a variable (like `__res`), then it must use "=".



**Error 18**:

In `buffer.c`, there is a macro called `COPYBLK`. It is a relatively simple one, so I'll copy paste the original code and the modified code so that you can check the difference. I never removed original code from the repo, but using a macro to route to my own version, so you can also check the repo.

Original: You know what, I really hate inline assembly wrapped with C macro.

```C
#define COPYBLK(from,to) \
__asm__("cld\n\t" \
	"rep\n\t" \
	"movsl\n\t" \
	::"c" (BLOCK_SIZE/4),"S" (from),"D" (to) \
	:"cx","di","si")
```

Modification: The only important issue, is that any **output** value inside of `()` must be a lvalue. So I can't say `"+c" (BLOCK_SIEZE/4)`. Instead, I need to introduce a temp variable to contain that value.

```C
/*
	Observations:
	- Both ESI and EDI require initial values and are changed frequently. Use "+".
	- ECX requires initial value and is decremented. Use "+".
	- "memory" in clobber.
*/

#define COPYBLK(from,to) \
do { \	
	size_t __c = BLOCK_SIZE / 4; \
	__asm__( \
		/* Clear DF. Subsequent string instructions increment ESI/EDI. */ \
		"cld\n\t" \
		/* Repeat movsl ECX times. Stop when ECX == 0. Decrement ECX for each repeat. */ \
		/* Move Long at address DS:(E)SI to address ES:(E)DI. */ \
		"rep\n\t" \
		"movsl\n\t" \
		:	"+S" (from), "+D" (to), "+c" (__c) \
		:	\
		:	"cc", "memory" \
	); \
} while (0)
```


**Error 19**:

The next file is `super.c`. I'm going to fix all inline assembly in one shot. Please check the repo for details. I'll only list the more interesting ones below.

```C
#define set_bit(bitnr,addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })
```

One quirk of this kind of multiple statement (GCC statement expression, I think this is the name.) is that I cannot wrap it with a `do...while(0)` loop, which makes me very itchy. The rest is pretty straightforward.

```C
/*
	BT %2, %3: 
		Selects the bit in a bit string (specified with the first operand, called the bit base) at the bit-position 
		designated by the bit offset (specified by the second operand) and stores the value of the bit in the CF flag.

	SETB %%al:
		Set byte if below (CF=1).

	Observations:
	- I can't wrap this with do...while (0).
	- "cc" in clobber list.
	- SETB %%AL changes AL to 0 or 1. So EAX is both input/output. I need to assign the initial value 0 to __res.
	- "r" is any register. "m" is memory location. They are both input.
*/

#define set_bit(bitnr,addr) ({ \
		int __res = 0; \
		__asm__( \
			"bt %1,%2;setb %%al" \
			:	"+a" (__res)	\
			:	"r" (bitnr), "m" (*(addr)) \
			: "cc" \
		); \
		__res; \
})
```

**Some thoughts on vibe coding**:

It is with a heavy heart that I learned that the author of "Ladybird Browser" managed to convert the Javascript compiler from C++ to Rush in 2 weeks, with the help of AI. It is a mix of awe and depression. 10x programmers leverages AI to achieve a great feat in only 2 weeks, and passing all tests. This is not a surprise to me, TBH, but reality hits hard still. I'm a very average programmer, a very average person, in all perspectives, and perhaps worse than then the median in many of them. The gap between an ordinary people, with a 10X whatever, is getting much larger due to the evolution of tools. No, I do not believe AI can ever replace humans completely, at least no in the near future, but the point is, we the ordinary people are getting less and less relevant. The gate of professional work, the gate from which we gain satisfaction by knowing that many are using our work, is closing. I have no ill feeling towards any 10X programmers who are leveraging the tools are enjoying this. They are much better than me. They have earned it. They deserve it. And I deserve it, too, because I have allowed myself to be mediocre. Being mediocre is a lesser evil then and now, but is a major sin in the future.

I soak myself in "Cryptozoologist" (Disco Elysium) to savor the moment. It is fine. Perhaps I will never get a professional job as a system programmer, and this is fine. I'll go into the woods, stay in a cave, and hack on my own projects, on my own terms. I do no care about the end products, and neither do I care whether people use them at all. Programming is a ritual to dispel the daemons from my soul, and I must keep doing it, until the last moment.

I'm also thinking about how to improve my technical skills. Many people say that technical people gotta build up their soft skills and business sense, which I do not disagree -- if I were to work on a product team. However, I'd like to regard myself as an amateur researcher -- an amateur indeed as neither did I have a proper education, and nor do I work in a highly competitive environment that promotes deep research -- nevertheless, a researcher I am. Instead of working on cutting edge techniques, I work on software archaeology. I take apart ancient artifacts and figure out how they run. There is no end-product in my mind. I don't produce any product that customers touch and use. I produce documents, comments and build guides for these artifacts which I am the only customer.

It requires a lot of patience to dig through the source code or the binary image to find whatever I need. From this perspecitve, upskilling is to build up my resistance to frustration -- because frustration is the air I gotta breathe. Upskilling is to recognize common patterns when I see one -- because all those operating systems share common design philosophy and algorithms. Upskilling means I need to spend time reading manuals, over and over; thinking through the problem, on paper and in my mind; talking to people on Stackoverflow or IRC, whatever their attitude is. It means getting frustrated, throwing keyboards away, and diving into rabbit holes forfeiting the hope to find anything useful after days of exploration. It means almost giving up every day, but keep coming back, knowing that there is no true ending, just milestones, until I expire from this world. There is no show but a nod to myself. The portfolios are not important -- it is the process to produce them that is important.

AI won't help me to upskill in this way. AI reduces fruatration -- but I need more of them to train my resistance. AI makes things more ordered, and more accessible to me -- but I need to learn how to untangle the chaos. The frustration, the untanglement -- they are where the fun is. Why should I handle the fun part to AI? I'm not in this game to find employment. I'm not in this game to impress anyone. It is the pinball game in "SOul of the new machine" -- I figure out this one, and reward myself with the next one.

AI does have its place, though. I need to expose myself to frustration for training -- but as muscles, the human mind cannot sustain too much of a frustration and it needs to rest. My current workflow seeks help from AI too quickly. There is little training if I resort to ChatGPT only after 1-2 hours of thinking. I decided to extend that period to 5 days. For any technical problem, I'll allow myself 5 full nights (preferably Mon-Fri) to figure it out, but will turn to ChatGPT if I fail to achieve so. I'll also use ChatGPT as a validator. I'm sure it bags more technical knowledge than me, so it doesn't hurt to have a second pair of eyes. The third usage is to write scripts in places that I have zero interests in -- for example, perhaps I need to downgrade a driver, but why should I take any interest in learning the bash commands? Instead I'll directly command ChatGPT to write one for me and be done with it. I need to focus on things that are truly important to me. Everything else, everyone else, is expendable.


**Error 20**:

This is a pretty weird one, as far as I think regarding C standard.

```
exec.c: In function 'copy_strings':
exec.c:163: error: invalid lvalue in assignment

```

The original code looks pretty cursed already. There are multiple assignments in the same `if` statement. I don't get it, what's the point of using assignment in `if`? Is it really that hard to do assignment on the side and simply `if` then assigned lvalue? Oh well...


```C
if (
	!(pag = (char *) page[p/PAGE_SIZE]) &&
	!(pag = (char *) page[p/PAGE_SIZE] = (unsigned long *) get_free_page())
)
	return 0;
```

Anyway, the first line of the `if` statement is OK. The second line is broken. `(char *) page[p/PAGE_SIZE] = (unsigned long *) get_free_page()` doesn't make sense, because ISO C standard says that "Cast does not yield an lvalue" (https://stackoverflow.com/questions/26470926/c-expression-must-be-a-modifiable-lvalue). However, (here is the good part), ChatGPT told me that GCC used to have a "cast as lvalue" extension in the early days (https://gcc.gnu.org/onlinedocs/gcc-3.4.6/gcc/Lvalues.html) but they deprecated it before 4.1. That's why it somehow worked back in the day, but doesn't work for later GCC versions. Here is my modification:

```C
				if (!(pag = (char *) page[p/PAGE_SIZE]))
				{
					page[p/PAGE_SIZE] = (unsigned long *) get_free_page();
					pag = (char *) page[p/PAGE_SIZE];
					
					if (!pag)
						return 0;
				}
```

**Error 21**:

This is not an error, but what I learned from reading GCC 4.1 manual about "cc" in the clobber list:

> If your assembler instruction can alter the condition code register, add `cc' to the list of clobbered registers. GCC on some machines represents the condition codes as a specific hardware register; `cc' serves to name this register. On other machines, the condition code is handled differently, and specifying `cc' has no effect. But it is valid no matter what the machine. 

