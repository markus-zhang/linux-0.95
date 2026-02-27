/*
 *  linux/fs/bitmap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux/sched.h>
#include <linux/minix_fs.h>
#include <linux/kernel.h>

#ifdef MARKUS_OUT

#define clear_block(addr) \
__asm__("cld\n\t" \
	"rep\n\t" \
	"stosl" \
	::"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)):"cx","di")

#else

// Observations: 
// - ECX is read/write, but BLOCK_SIZE/4 is not an lvalue (which is required to write into), so needs a temp var.
// - EDI is read/write, (long)(addr) is a cast so probably does not suite for an lavalue, so needs a temp var.
// - "cc" and "memory" in clobber list.
// - STOSL loads EAX into the address stored in (E)DI.

#define clear_block(addr) \
do { \
	unsigned long __c = (unsigned long)BLOCK_SIZE / 4; \
	unsigned long __D = (unsigned long)(addr); \
	__asm__( \
		"cld\n\t" \
		"rep\n\t" \
		"stosl" \
		: "+c" (__c), "+D" (__D) \
		:	"a" (0) \
		:	"cc","memory"\
	); \
} while (0)

#endif

#ifdef MARKUS_OUT

#define set_bit(nr,addr) ({\
char res; \
__asm__ __volatile__("btsl %1,%2\n\tsetb %0": \
"=q" (res):"r" (nr),"m" (*(addr))); \
res;})

#else

// Observations:
// - "cc" in clobber list.
// - "=q" is fine because SETB only sets, not reads. So it's just an output.
// - "m" is both input/output (modified by BTSL), so it should be "+m".
//	- Note that since I swapped the order of m and r, I swapped %1 and %2 too in BTSL.
// - Cannot use do...while(0) loop for statement expression.

#define set_bit(nr,addr) \
({ \
	char res; \
	__asm__ __volatile__( \
		/* Select the bit in a bit string addr at the bit-position designated by nr */ \
		/* Store the selected bit in the CF flag, and sets the selected bit in the bit string to 1. */ \
		"btsl %2,%1\n\t" \
		/* Set byte if CF = 1. This is equivalent to -- set %0 to the previously selected bit ^. */ \
		/* Case 1: CF = 1, which means the previously selected bit is 1, so we set %0 to its value. */ \
		/* Case 2: CF = 0, which means the previously selected bit is 0, so we set %0 to its value. */ \
		"setb %0" \
		/* q means a, b, c or d register for i386. */ \
		:	"=q" (res), "+m" (*(addr)) \
		:	"r" (nr) \
		: "cc" \
	); \
	res; \
})

#endif

#ifdef MARKUS_OUT

#define clear_bit(nr,addr) ({\
char res; \
__asm__ __volatile__("btrl %1,%2\n\tsetnb %0": \
"=q" (res):"r" (nr),"m" (*(addr))); \
res;})

#else

// Observations:
// - Similar to ^, "r" is input only, and "m" is both input/output. So "m" becomes "+m".
//	- And because of this, I need to swap %1, %2 in BTRL.
// - "cc" in the clobber list.

#define clear_bit(nr,addr) \
({ \
	char res; \
	__asm__ __volatile__( \
		/* Select the bit in a bit string addr at the bit-position designated by nr */ \
		/* Store the selected bit in the CF flag, and clears the selected bit in the bit string to 0. */ \
		"btrl %2,%1\n\t" \
		/* Set byte if CF = 0.  */ \
		"setnb %0" \
		: "=q" (res), "+m" (*(addr)) \
		:	"r" (nr) \
		: "cc" \
	); \
	res; \
})

#endif

#ifdef MARKUS_OUT

#define find_first_zero(addr) ({ \
int __res; \
__asm__("cld\n" \
	"1:\tlodsl\n\t" \
	"notl %%eax\n\t" \
	"bsfl %%eax,%%edx\n\t" \
	"jne 2f\n\t" \
	"addl $32,%%ecx\n\t" \
	"cmpl $8192,%%ecx\n\t" \
	"jl 1b\n\t" \
	"xorl %%edx,%%edx\n" \
	"2:\taddl %%edx,%%ecx" \
	:"=c" (__res):"0" (0),"S" (addr):"ax","dx","si"); \
__res;})

#else

/**
 * 
 * Usage in bitmap.c: find_fist_zero() loops through b_data (an array of 1024 bytes, which is 8192 bits):
 *	for each element of b_data, find its first zero.
 *	If found then break, otherwise go to the next element.
 * Observations:
 * - Keep "=c" in output list and "0" in input list.
 * - ESI definitely needs a "+". It's both read/write. 
 * - EAX is not an input, and it gets written into. It's fine to keep it in the clobber list.
 * - Same for EDX.
 * - "cc" and "memory" in clobber list.
*/

#define find_first_zero(addr) ({ \
int __res; \
__asm__(	\
	/* Clears DF. Subsequent string instructions  */ \
	"cld\n" \
	/* Load 32-bit at (E)SI (addr) into EAX. Increment ESI afterwards as DF is cleared. */ \
	"1:\tlodsl\n\t" \
	/* NOT EAX first (see below for why), and then BSFL scans EAX for the least significant set bit (1 bit). */ \
	/* If found, store index of that 1 bit into EDX. */ \
	/* So apparently x86 doesn't have a scan for 0 instruction, so Linus had to NOT and find 1 instead. */ \
	/* Set ZF to 1 if ~EAX is 0. */ \
	"notl %%eax\n\t" \
	"bsfl %%eax,%%edx\n\t" \
	/* If ZF != 0 (~EAX is all 0), jump forward to label 2:. */ \
	"jne 2f\n\t" \
	/* Add 32 onto ECX (__res, which is initialized as 0), This seems to be a loop counter. */ \
	/* Each loop increments ECX by 32 until ECX >= 8192 (see below). */ \
	"addl $32,%%ecx\n\t" \
	/* 8192 is 0x2000. This is to compare ECX with 0x2000. */ \
	"cmpl $8192,%%ecx\n\t" \
	/* If ECX < 0x2000, jump back to label 1:. */ \
	"jl 1b\n\t" \
	/* If ECX >= 0x2000, clear EDX. */ \
	"xorl %%edx,%%edx\n" \
	/* Add EDX onto ECX. Two cases: */ \
	/* Case 1: ~EAX is all 0, i.e. EAX is all 1, there is no zero. EDX = 0. EDX + ECX = 0. */ \
	/* Case 2: ~EAX is not all 0, i.e. some bits of EAX is 0. EDX = index of first 0. ECX = EDX + ECX. */ \
	/* Case 2: So for example, if EDX is the 4th bit, then ECX = 4 + 32 * 4. */ \
	/* It might have sth. to do with the FS. Apparently this is for bitmap operations. */ \
	/* I think it's because in bitmap operations Linux needed to do this: */ \
	/* if ((j=find_first_zero(bh->b_data))<8192)
				break; */ \
	/* And b_data is an array of 1024 bytes, so 1024 * 8 = 8192 bytes. That's why he compares ECX with 8092. */ \
	"2:\taddl %%edx,%%ecx" \
	:	"=c" (__res), "+S" (addr) \
	:	"0" (0) \
	:	"ax","dx","cc", "memory" \
); \
__res; \
})

#endif

int minix_free_block(int dev, int block)
{
	struct super_block * sb;
	struct buffer_head * bh;
	unsigned int bit,zone;

	if (!(sb = get_super(dev)))
		panic("trying to free block on nonexistent device");
	if (block < sb->s_firstdatazone || block >= sb->s_nzones)
		panic("trying to free block not in datazone");
	bh = get_hash_table(dev,block);
	if (bh) {
		if (bh->b_count > 1) {
			brelse(bh);
			return 0;
		}
		bh->b_dirt=0;
		bh->b_uptodate=0;
		if (bh->b_count)
			brelse(bh);
	}
	zone = block - sb->s_firstdatazone + 1;
	bit = zone & 8191;
	zone >>= 13;
	bh = sb->s_zmap[zone];
	if (clear_bit(bit,bh->b_data))
		printk("free_block (%04x:%d): bit already cleared\n",dev,block);
	bh->b_dirt = 1;
	return 1;
}

int minix_new_block(int dev)
{
	struct buffer_head * bh;
	struct super_block * sb;
	int i,j;

	if (!(sb = get_super(dev)))
		panic("trying to get new block from nonexistant device");
	j = 8192;
	for (i=0 ; i<8 ; i++)
		if (bh=sb->s_zmap[i])
			if ((j=find_first_zero(bh->b_data))<8192)
				break;
	if (i>=8 || !bh || j>=8192)
		return 0;
	if (set_bit(j,bh->b_data))
		panic("new_block: bit already set");
	bh->b_dirt = 1;
	j += i*8192 + sb->s_firstdatazone-1;
	if (j >= sb->s_nzones)
		return 0;
	if (!(bh=getblk(dev,j)))
		panic("new_block: cannot get block");
	if (bh->b_count != 1)
		panic("new block: count is != 1");
	clear_block(bh->b_data);
	bh->b_uptodate = 1;
	bh->b_dirt = 1;
	brelse(bh);
	return j;
}

void minix_free_inode(struct inode * inode)
{
	struct buffer_head * bh;

	if (!inode)
		return;
	if (!inode->i_dev) {
		memset(inode,0,sizeof(*inode));
		return;
	}
	if (inode->i_count>1) {
		printk("free_inode: inode has count=%d\n",inode->i_count);
		return;
	}
	if (inode->i_nlink) {
		printk("free_inode: inode has nlink=%d\n",inode->i_nlink);
		return;
	}
	if (!inode->i_sb) {
		printk("free_inode: inode on nonexistent device\n");
		return;
	}
	if (inode->i_ino < 1 || inode->i_ino > inode->i_sb->s_ninodes) {
		printk("free_inode: inode 0 or nonexistent inode\n");
		return;
	}
	if (!(bh=inode->i_sb->s_imap[inode->i_ino>>13])) {
		printk("free_inode: nonexistent imap in superblock\n");
		return;
	}
	if (clear_bit(inode->i_ino&8191,bh->b_data))
		printk("free_inode: bit already cleared.\n\r");
	bh->b_dirt = 1;
	memset(inode,0,sizeof(*inode));
}

struct inode * minix_new_inode(int dev)
{
	struct inode * inode;
	struct buffer_head * bh;
	int i,j;

	if (!(inode=get_empty_inode()))
		return NULL;
	if (!(inode->i_sb = get_super(dev))) {
		printk("new_inode: unknown device\n");
		iput(inode);
		return NULL;
	}
	j = 8192;
	for (i=0 ; i<8 ; i++)
		if (bh=inode->i_sb->s_imap[i])
			if ((j=find_first_zero(bh->b_data))<8192)
				break;
	if (!bh || j >= 8192 || j+i*8192 > inode->i_sb->s_ninodes) {
		iput(inode);
		return NULL;
	}
	if (set_bit(j,bh->b_data)) {	/* shouldn't happen */
		printk("new_inode: bit already set");
		iput(inode);
		return NULL;
	}
	bh->b_dirt = 1;
	inode->i_count = 1;
	inode->i_nlink = 1;
	inode->i_dev = dev;
	inode->i_uid = current->euid;
	inode->i_gid = current->egid;
	inode->i_dirt = 1;
	inode->i_ino = j + i*8192;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &minix_inode_operations;
	return inode;
}
