#include <mmu.h>
#include <memlayout.h>

#define REALLOC(x) (x - KERNBASE)

# before kern_entry lab2 stage 1： virt addr = linear addr = phy addr
.text
.globl kern_entry
kern_entry:
    # load pa of boot pgdir
    # __boot_pgdir - kernbase
    movl $REALLOC(__boot_pgdir), %eax
    movl %eax, %cr3
    # before enable paging, lab2 stage 2： virt addr - 0xC0000000 = linear addr = phy addr

    # enable paging
    movl %cr0, %eax
    orl $(CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP), %eax
    andl $~(CR0_TS | CR0_EM), %eax
    movl %eax, %cr0
    # Now CPU starts to enable page.

    # LEA (load effective address) just computes the address of the operand, it does not actually dereference it
    # LEA assigns to the %eax register the value of "next". c010001e.
    # https://stackoverflow.com/questions/11212444/what-does-the-leal-assembly-instruction-do
    # update eip, so ucore goes to high-end virtual space
    # now, eip = 0x1.....(which is the value not updated yet) why not just jmp next ????
    leal next, %eax

    # https://en.wikipedia.org/wiki/JMP_(x86_instruction) JMP loads the IP register with the new value
    # set eip = KERNBASE + 0x1.....
    jmp *%eax
next:

    # unmap va 0 ~ 4M, it's temporary mapping
    # this is to unmap the temporary mapping. 
    xorl %eax, %eax
    movl %eax, __boot_pgdir

    # set ebp, esp
    movl $0x0, %ebp
    # the kernel stack region is from bootstack -- bootstacktop,
    # the kernel stack size is KSTACKSIZE (8KB)defined in memlayout.h
    movl $bootstacktop, %esp

    # now we set up the mapping within 0~4M
    # virt addr = linear addr = phy addr + 0xC0000000
    # now kernel stack is ready , call the first C function
    call kern_init

# should never get here
spin:
    jmp spin

.data
.align PGSIZE
    .globl bootstack
bootstack:
    .space KSTACKSIZE
    .globl bootstacktop
bootstacktop:

# kernel builtin pgdir
# an initial page directory (Page Directory Table, PDT)
# These page directory table and page table can be reused!
.section .data.pgdir
.align PGSIZE
__boot_pgdir:
.globl __boot_pgdir
    # TODO: need to understand. why there're two mappings???
    # I know this is used after paging is enabled. but how???
    # 1st one is one-one mapping.
    # map va 0 ~ 4M to pa 0 ~ 4M (temporary)
    .long REALLOC(__boot_pt1) + (PTE_P | PTE_U | PTE_W)
    .space (KERNBASE >> PGSHIFT >> 10 << 2) -    (. - __boot_pgdir) # pad to PDE of KERNBASE
    # 2nd one is mapping va KERNBASE + (0 ~ 4M) to pa 0 ~ 4M
    .long REALLOC(__boot_pt1) + (PTE_P | PTE_U | PTE_W)
    .space PGSIZE - (. - __boot_pgdir) # pad to PGSIZE

.set i, 0
__boot_pt1:
.rept 1024
    .long i * PGSIZE + (PTE_P | PTE_W)
    .set i, i + 1
.endr

