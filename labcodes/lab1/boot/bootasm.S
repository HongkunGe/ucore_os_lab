#include <asm.h>

# This is part 1 of boot loader
# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        0x8                     # kernel code segment selector
.set PROT_MODE_DSEG,        0x10                    # kernel data segment selector
.set CR0_PE_ON,             0x1                     # protected mode enable flag https://en.wikipedia.org/wiki/Control_register#CR0

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
seta20.1:
    # Wait for not busy(8042 input buffer empty), Input buffer status (0 = empty, 1 = full).
    # http://www.independent-software.com/writing-your-own-toy-operating-system-enabling-the-a20-line/
    # https://en.wikipedia.org/wiki/TEST_(x86_instruction)
    # http://faydoc.tripod.com/cpu/jnz.htm
    # https://wiki.osdev.org/%228042%22_PS/2_Controller
    inb $0x64, %al                                  # Read one byte from status register of 8042 to al register.
    testb $0x2, %al                                 # bit#1 is 1, input register has data; testb ($0x2 AND %al(1)) is 1, ZF is set to 0.
    jnz seta20.1                                    # jump to seta20.1 if ZF = 0.

    # this is to send a command to the controller, simply by writing the command byte to IO port 0x64.
    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # cmd type 0xd1 means: write data to 8042's P2 port(Controller Output Port)

seta20.2:
    # Check PS/2 Controller IO Ports in https://wiki.osdev.org/%228042%22_PS/2_Controller
    # Still read from 0x64, b/c it's the Status Register.
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    # Write data to input buffer.
    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.

    # https://en.wikibooks.org/wiki/X86_Assembly/Global_Descriptor_Table
    # Load GDT, gdtdesc is a pointer to 6 bytes of memory containing the desired GDTR value.
    # 0-15 is LIMIT; 16-47 is BASE.
    # https://en.wikipedia.org/wiki/Control_register see CR0 0th bit is PE.
    # If 1, system is in protected mode, else system is in real mode
    lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax  #ENABLE protected mode by setting PE flag.
    movl %eax, %cr0

    # The functionality of CS segment register is largely changed after entering protection mode. Before that, CS serves
    # as a base address(CS * 16 + IP). After that, it is a segment selector, which's used as an index of GDT.
    # ljmp must be executed right after mov, to change CS register to the selector.
    # ljmp的含义是长跳，长跳主要就是重新加载寄存器，32位保护模式主要体现在段寄存器，具有可以参考段选择子和段描述符的概念，
    # 如果不用长跳的话，那么段寄存器不会重新加载，后面的取指结果仍然是老段寄存器中的值，当然保护模式不会生效了
    # https://piazza.com/class/i5j09fnsl7k5x0?cid=777

    # Jump to next instruction, but in 32-bit code segment. Now only assembly that
    # operates on CS register can be working fine.(??)
    # Switches processor into 32-bit mode.
    # $PROT_MODE_CSEG is provided for CS register, while $protcseg is for EIP.
    # http://csiflabs.cs.ucdavis.edu/~ssdavis/50/att-syntax.htm
    # https://docs.oracle.com/cd/E19455-01/806-3773/instructionset-73/index.html
    # jmp far to a different code segment using segment-offset pointer. CS segment + $protcseg(offset)
    ljmp $PROT_MODE_CSEG, $protcseg
    # ^^ understand this command!!!
    # $PROT_MODE_CSEG is 0x8 serving as index of GDT, 0x8 will be put in the CS register.
    # CPU will compute the address using GDT.at(0x8 * 1bytes) + EIP(offset); note we we read the descriptor from GDT at(0x8 * 1bytes);
    # We fetch the base address from the descriptor, 0x0. suppose $protcseg is 0x7c32. So ljmp jumps to 0x7c32.
    ## if GDT have N descriptors, this GDT will take space of 8N bytes. Start base is gdt_base, the end address will be gdt_base + 8N -1.
    # GDTR(48 bits) segment limit use byte as unit. high 32 bits is gdt_base, base of GDT. low 16 bits is used as the segment limit, using byte as unit.
.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
    # initialize all segment registers with PROT_MODE_DSEG(0x10). They all point to segment descriptor in GDT at 0x10(data segment descriptor).
    # why could they point to data segment descriptor? b/c it's determined by the segmentation machinasm in https://chyyuu.gitbooks.io/simple_os_book/zh/chapter-1/protect_mode.html.
    # Item 6. https://chyyuu.gitbooks.io/simple_os_book/zh/chapter-1/real_mode_switch_protect_mode.html
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain

    # If bootmain returns (it shouldn't), loop.
spin:
    jmp spin

# Bootstrap GDT
# https://stackoverflow.com/questions/21546946/what-does-p2align-do-in-asm-code
# https://sourceware.org/binutils/docs/as/P2align.html#P2align
# Each segment descriptor takes 8 bytes.
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel

# GDT register 48 bits; lower 16 bits tell the size of GDT. 0x17 + 1 = 0x18 (24).
gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt
