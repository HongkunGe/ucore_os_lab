#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include <defs.h>

#define do_div(n, base) ({                                        \
    unsigned long __upper, __low, __high, __mod, __base;        \
    __base = (base);                                            \
    asm("" : "=a" (__low), "=d" (__high) : "A" (n));            \
    __upper = __high;                                            \
    if (__high != 0) {                                            \
        __upper = __high % __base;                                \
        __high = __high / __base;                                \
    }                                                            \
    asm("divl %2" : "=a" (__low), "=d" (__mod)                    \
        : "rm" (__base), "0" (__low), "1" (__upper));            \
    asm("" : "=A" (n) : "a" (__low), "d" (__high));                \
    __mod;                                                        \
 })

static inline uint8_t inb(uint16_t port) __attribute__((always_inline));
static inline void insl(uint32_t port, void *addr, int cnt) __attribute__((always_inline));
static inline void outb(uint16_t port, uint8_t data) __attribute__((always_inline));
static inline void outw(uint16_t port, uint16_t data) __attribute__((always_inline));
static inline uint32_t read_ebp(void) __attribute__((always_inline));

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
struct pseudodesc {
    uint16_t pd_lim;        // Limit
    uint32_t pd_base;        // Base address
} __attribute__ ((packed));

static inline void lidt(struct pseudodesc *pd) __attribute__((always_inline));
static inline void sti(void) __attribute__((always_inline));
static inline void cli(void) __attribute__((always_inline));
static inline void ltr(uint16_t sel) __attribute__((always_inline));

// read from I/O device to %eax
static inline uint8_t
inb(uint16_t port) {
    uint8_t data;
    asm volatile ("inb %1, %0" : "=a" (data) : "d" (port));
    return data;
}

/**
 * The function transfers string from port(like I/O disk) specified DX register(@param port)
 * to the memory byte pointed by ES:destination index. destination index is specified
 * by EDI register (@param addr)
 *
 * Clear DF; DF = 0
 *
 * https://c9x.me/x86/html/file_module_x86_id_279.html
 * https://www.aldeid.com/wiki/X86-assembly/Instructions/repne
 * repne PREFIX can be added to INS instruction.
 * Repeats a string instruction the number of times specified in the count register ((E)CX)
 * or until the indicated condition of the ZF flag is no longer met.
 * while(ecx != 0) // ecx is cnt <= both input and output
 * {
 * 		ZF = (al == *(BYTE *)edi);
 * 		if(DF == 0) edi++; // edi is set by "=D"(addr), also it's both output/input.
 * 		else edi--;
 * 		ecx--;
 * 		if(ZF) break;
 * 	}
 *
 * 	https://docs.oracle.com/cd/E19455-01/806-3773/instructionset-6/index.html
 * 	INS instruction transfers a string from a port specified in the DX register ("d"(port) below)
 * 	to the memory byte or word pointed to by the ES:destination index.
 * 	Load the desired port number into the DX register and the desired destination address
 * 	into the DI or EDI index register("=D"(addr) below) before executing the ins instruction.
 * 	After a transfer occurs, the destination-index register is automatically incremented or decremented
 * 	as determined by the value of the direction flag (DF).
 * 	The index register is incremented if DF = 0 (DF cleared by a cld instruction);
 * 	it is decremented if DF = 1 (DF set by a std instruction).
 * 	The increment or decrement count is 1 for a byte transfer, 2 for a word, and 4 for a long.
 * 	Use the rep prefix with the ins instruction for a block transfer of CX bytes or words.
 * */
static inline void
insl(uint32_t port, void *addr, int cnt) {
    asm volatile (
            "cld;"
            "repne; insl;"
            : "=D" (addr), "=c" (cnt)
            : "d" (port), "0" (addr), "1" (cnt)
            : "memory", "cc"); // This is to inform gcc that we will use and modify them ourselves.
    // So gcc will not assume that the values it loads into these registers will be valid.
}

static inline void
outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" :: "a" (data), "d" (port));
}

static inline void
outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port));
}

static inline uint32_t
read_ebp(void) {
    uint32_t ebp;
    asm volatile ("movl %%ebp, %0" : "=r" (ebp));
    return ebp;
}

static inline void
lidt(struct pseudodesc *pd) {
    asm volatile ("lidt (%0)" :: "r" (pd));
}

static inline void
sti(void) {
    asm volatile ("sti");
}

static inline void
cli(void) {
    asm volatile ("cli");
}

static inline void
ltr(uint16_t sel) {
    asm volatile ("ltr %0" :: "r" (sel));
}

static inline int __strcmp(const char *s1, const char *s2) __attribute__((always_inline));
static inline char *__strcpy(char *dst, const char *src) __attribute__((always_inline));
static inline void *__memset(void *s, char c, size_t n) __attribute__((always_inline));
static inline void *__memmove(void *dst, const void *src, size_t n) __attribute__((always_inline));
static inline void *__memcpy(void *dst, const void *src, size_t n) __attribute__((always_inline));

#ifndef __HAVE_ARCH_STRCMP
#define __HAVE_ARCH_STRCMP
static inline int
__strcmp(const char *s1, const char *s2) {
    int d0, d1, ret;
    asm volatile (
            "1: lodsb;"
            "scasb;"
            "jne 2f;"
            "testb %%al, %%al;"
            "jne 1b;"
            "xorl %%eax, %%eax;"
            "jmp 3f;"
            "2: sbbl %%eax, %%eax;"
            "orb $1, %%al;"
            "3:"
            : "=a" (ret), "=&S" (d0), "=&D" (d1)
            : "1" (s1), "2" (s2)
            : "memory");
    return ret;
}

#endif /* __HAVE_ARCH_STRCMP */

#ifndef __HAVE_ARCH_STRCPY
#define __HAVE_ARCH_STRCPY
static inline char *
__strcpy(char *dst, const char *src) {
    int d0, d1, d2;
    asm volatile (
            "1: lodsb;"
            "stosb;"
            "testb %%al, %%al;"
            "jne 1b;"
            : "=&S" (d0), "=&D" (d1), "=&a" (d2)
            : "0" (src), "1" (dst) : "memory");
    return dst;
}
#endif /* __HAVE_ARCH_STRCPY */

#ifndef __HAVE_ARCH_MEMSET
#define __HAVE_ARCH_MEMSET
static inline void *
__memset(void *s, char c, size_t n) {
    int d0, d1;
    asm volatile (
            "rep; stosb;"
            : "=&c" (d0), "=&D" (d1)
            : "0" (n), "a" (c), "1" (s)
            : "memory");
    return s;
}
#endif /* __HAVE_ARCH_MEMSET */

#ifndef __HAVE_ARCH_MEMMOVE
#define __HAVE_ARCH_MEMMOVE
static inline void *
__memmove(void *dst, const void *src, size_t n) {
    if (dst < src) {
        return __memcpy(dst, src, n);
    }
    int d0, d1, d2;
    asm volatile (
            "std;"
            "rep; movsb;"
            "cld;"
            : "=&c" (d0), "=&S" (d1), "=&D" (d2)
            : "0" (n), "1" (n - 1 + src), "2" (n - 1 + dst)
            : "memory");
    return dst;
}
#endif /* __HAVE_ARCH_MEMMOVE */

#ifndef __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMCPY
static inline void *
__memcpy(void *dst, const void *src, size_t n) {
    int d0, d1, d2;
    asm volatile (
            "rep; movsl;"
            "movl %4, %%ecx;"
            "andl $3, %%ecx;"
            "jz 1f;"
            "rep; movsb;"
            "1:"
            : "=&c" (d0), "=&D" (d1), "=&S" (d2)
            : "0" (n / 4), "g" (n), "1" (dst), "2" (src)
            : "memory");
    return dst;
}
#endif /* __HAVE_ARCH_MEMCPY */

#endif /* !__LIBS_X86_H__ */

