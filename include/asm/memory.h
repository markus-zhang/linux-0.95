/*
 *  NOTE!!! memcpy(dest,src,n) assumes ds=es=normal data segment. This
 *  goes for all kernel functions (ds=es=kernel space, fs=local data,
 *  gs=null), as well as for all well-behaving user programs (ds=es=
 *  user data space). This is NOT a bug, as any user program that changes
 *  es deserves to die if it isn't careful.
 */

#ifdef MARKUS_OUT

#define memcpy(dest,src,n) ({ \
void * _res = dest; \
__asm__ ("cld;rep;movsb" \
	::"D" ((long)(_res)),"S" ((long)(src)),"c" ((long) (n)) \
	:"di","si","cx"); \
_res; \
})

#else

// Observations:
// - ESI and EDI are both input/output. Use "+" and temp vars.
// 	- In usage dest and src are always char *, but looks like I need to cast to long.
// - ECX is both input/output. Use "+" and temp var.
// - "cc" and "memory" in clobber list.

#define memcpy(dest,src,n) ({ \
	void *_res = dest; \
	char * __dest = (char *)dest; \
	char * __src = (char *)src; \
	unsigned long __n = (unsigned long)n; \
	__asm__ ( \
		/* MOVSB: Move byte from address (E)SI to address (E)DI. */ \
		"cld;rep;movsb" \
		: "+D" (__dest),"+S" (__src), "+c" (__n) \
		:	\
		:	"cc", "memory" \
	); \
	_res; \
})

#endif