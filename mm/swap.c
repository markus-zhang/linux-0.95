/*
 *  linux/mm/swap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This file should contain most things doing the swapping from/to disk.
 * Started 18.12.91
 */

#include <string.h>
#include <errno.h>

#include <linux/mm.h>
#include <sys/stat.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

#define SWAP_BITS (4096<<3)

#define bitop(name,op) \
static inline int name(char * addr,unsigned int nr) \
{ \
int __res; \
__asm__ __volatile__("bt" op " %1,%2; adcl $0,%0" \
:"=g" (__res) \
:"r" (nr),"m" (*(addr)),"0" (0)); \
return __res; \
}

bitop(bit,"")
bitop(setbit,"s")
bitop(clrbit,"r")

static char * swap_bitmap = NULL;
unsigned int swap_device = 0;
struct inode * swap_file = NULL;

void rw_swap_page(int rw, unsigned int nr, char * buf)
{
	unsigned int zones[4];
	int i;

	if (swap_device) {
		ll_rw_page(rw,swap_device,nr,buf);
		return;
	}
	if (swap_file) {
		nr <<= 2;
		for (i = 0; i < 4; i++)
			if (!(zones[i] = bmap(swap_file,nr++))) {
				printk("rw_swap_page: bad swap file\n");
				return;
			}
		ll_rw_swap_file(rw,swap_file->i_dev, zones,4,buf);
		return;
	}
	printk("ll_swap_page: no swap file or device\n");
}

/*
 * We never page the pages in task[0] - kernel memory.
 * We page all other pages.
 */
#define FIRST_VM_PAGE (TASK_SIZE>>12)
#define LAST_VM_PAGE (1024*1024)
#define VM_PAGES (LAST_VM_PAGE - FIRST_VM_PAGE)

static int get_swap_page(void)
{
	int nr;

	if (!swap_bitmap)
		return 0;
	for (nr = 1; nr < SWAP_BITS ; nr++)
		if (clrbit(swap_bitmap,nr))
			return nr;
	return 0;
}

void swap_free(int swap_nr)
{
	if (!swap_nr)
		return;
	if (swap_bitmap && swap_nr < SWAP_BITS)
		if (!setbit(swap_bitmap,swap_nr))
			return;
	printk("swap_free: swap-space bitmap bad\n");
	return;
}

void swap_in(unsigned long *table_ptr)
{
	int swap_nr;
	unsigned long page;

	if (!swap_bitmap) {
		printk("Trying to swap in without swap bit-map");
		return;
	}
	if (1 & *table_ptr) {
		printk("trying to swap in present page\n\r");
		return;
	}
	swap_nr = *table_ptr >> 1;
	if (!swap_nr) {
		printk("No swap page in swap_in\n\r");
		return;
	}
	if (!(page = get_free_page()))
		oom();
	read_swap_page(swap_nr, (char *) page);
	if (setbit(swap_bitmap,swap_nr))
		printk("swapping in multiply from same page\n\r");
	*table_ptr = page | (PAGE_DIRTY | 7);
}

int try_to_swap_out(unsigned long * table_ptr)
{
	unsigned long page;
	unsigned long swap_nr;

	page = *table_ptr;
	if (!(PAGE_PRESENT & page))
		return 0;
	if (page - LOW_MEM > PAGING_MEMORY)
		return 0;
	if (PAGE_DIRTY & page) {
		page &= 0xfffff000;
		if (mem_map[MAP_NR(page)] != 1)
			return 0;
		if (!(swap_nr = get_swap_page()))
			return 0;
		*table_ptr = swap_nr<<1;
		invalidate();
		write_swap_page(swap_nr, (char *) page);
		free_page(page);
		return 1;
	}
	page &= 0xfffff000;
	*table_ptr = 0;
	invalidate();
	free_page(page);
	return 1;
}

/*
 * Go through the page tables, searching for a user page that
 * we can swap out.
 */
int swap_out(void)
{
	static int dir_entry = 1024;
	static int page_entry = -1;
	int counter = VM_PAGES;
	int pg_table = 0;

repeat:
	while (counter > 0) {
		counter -= 1024;
		dir_entry++;
		if (dir_entry >= 1024)
			dir_entry = FIRST_VM_PAGE>>10;
		if (pg_table = pg_dir[dir_entry])
			break;
	}
	if (counter <= 0) {
		printk("Out of swap-memory\n");
		return 0;
	}
	if (!(pg_table & 1)) {
		printk("bad page-table at pg_dir[%d]: %08x\n\r",dir_entry,
			pg_table);
		return 0;
	}
	pg_table &= 0xfffff000;
	while (counter > 0) {
		counter--;
		page_entry++;
		if (page_entry >= 1024) {
			page_entry = -1;
			goto repeat;
		}
		if (try_to_swap_out(page_entry + (unsigned long *) pg_table))
			return 1;
	}
	printk("Out of swap-memory\n\r");
	return 0;
}

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
unsigned long get_free_page(void)
{
	unsigned long result;

	unsigned long __paging_pages = PAGING_PAGES;
	unsigned long __edi = mem_map+PAGING_PAGES-1;

repeat:
	#ifdef MARKUS_OUT
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
	#else
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
	#endif
	if (result >= HIGH_MEMORY)
		goto repeat;
	if ((result && result < LOW_MEM) || (result & 0xfff)) {
		printk("weird result: %08x\n",result);
		result = 0;
	}
	if (!result && swap_out())
		goto repeat;
	return result;
}

/*
 * Written 01/25/92 by Simmule Turner, heavily changed by Linus.
 *
 * The swapon system call
 */

int sys_swapon(const char * specialfile)
{
	struct inode * swap_inode;
	int i,j;

	if (!suser())
		return -EPERM;
	if (!(swap_inode  = namei(specialfile)))
		return -ENOENT;
	if (swap_file || swap_device || swap_bitmap) {
		iput(swap_inode);
		return -EBUSY;
	}
	if (S_ISBLK(swap_inode->i_mode)) {
		swap_device = swap_inode->i_rdev;
		iput(swap_inode);
	} else if (S_ISREG(swap_inode->i_mode))
		swap_file = swap_inode;
	else {
		iput(swap_inode);
		return -EINVAL;
	}
	swap_bitmap = (char *) get_free_page();
	if (!swap_bitmap) {
		iput(swap_file);
		swap_device = 0;
		swap_file = NULL;
		printk("Unable to start swapping: out of memory :-)\n");
		return -ENOMEM;
	}
	read_swap_page(0,swap_bitmap);
	if (strncmp("SWAP-SPACE",swap_bitmap+4086,10)) {
		printk("Unable to find swap-space signature\n\r");
		free_page((long) swap_bitmap);
		iput(swap_file);
		swap_device = 0;
		swap_file = NULL;
		swap_bitmap = NULL;
		return -EINVAL;
	}
	memset(swap_bitmap+4086,0,10);
	j = 0;
	for (i = 1 ; i < SWAP_BITS ; i++)
		if (bit(swap_bitmap,i))
			j++;
	if (!j) {
		printk("Empty swap-file\n");
		free_page((long) swap_bitmap);
		iput(swap_file);
		swap_device = 0;
		swap_file = NULL;
		swap_bitmap = NULL;
		return -EINVAL;
	}
	printk("Adding Swap: %d pages (%d bytes) swap-space\n\r",j,j*4096);
	return 0;
}
