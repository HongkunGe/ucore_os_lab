#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <x86.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <kdebug.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n",TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* *
 * Interrupt descriptor table:
 *
 * Must be built at run time because shifted function addresses can't
 * be represented in relocation records.
 * */
static struct gatedesc idt[256] = {{0}};

static struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

static void
lab1_switch_to_user(void) {
    //LAB1 CHALLENGE 1 : TODO
    asm volatile (
    "sub $0x8, %%esp \n" // subtract esp(stack pointer) by 0x8.(8bytes)
    "int %0 \n" // after this line, we are in the user mode.
    // "movl %%ebp, %%esp \n" // without -8 in trap.c L221. esp will be same as esp in kernel mode before going to ISR.
    :
    : "i" (T_SWITCH_TOU)
    );
}

static void
lab1_switch_to_kernel(void) {
    //LAB1 CHALLENGE 1 :  TODO
    uint32_t reg1, reg2, reg3, reg4;
    asm volatile (
    "int %4 \n"
    "movl %%ebp, %0 \n"
    "movl %%esp, %1 \n"
    "movl %%ebp, %%esp \n"
    "movl %%ebp, %2 \n"
    "movl %%esp, %3 \n"
    : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4)
    : "i" (T_SWITCH_TOK)
    );

    cprintf(" = %x\n", reg1);
    cprintf(" = %x\n", reg2);
    cprintf(" = %x\n", reg3);
    cprintf(" = %x\n", reg4);
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void
idt_init(void) {
     /* LAB1 YOUR CODE : STEP 2 */
     /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */
     extern uintptr_t __vectors[];

     // 256, 2048, 8bytes; gate descriptor 8bytes
     // cprintf("%d, %d, %d", sizeof(idt) / sizeof(struct gatedesc), sizeof(idt), sizeof(struct gatedesc));
     // Why __vectors[i] give us the offset ??

     //TODO: when will interrupt gate be constructed?? All gates are trap gates. Why??
     // Why always no trap? All traps are handled as interruption. So ALL gates are interrupt gate. IF flag are always cleared
     // and interruption are always disabled during ISR of trap/interruption.
     // 当控制权通过中断门进入中断处理程序时，处理器清IF标志，即关中断，以避免嵌套中断的发生。中断门中的DPL（Descriptor Privilege Level）为0
     // 因此用户态的进程不能访问中断门。所有的中断处理程序都由中断门激活，并全部限制在内核态。

     // 在中断门描述符表中通过建立中断门描述符，其中存储了中断处理例程的代码段GD_KTEXT
     // 和偏移量\__vectors[i]，特权级为DPL_KERNEL。这样通过查询idt[i]就可定位到中断服务例程的起始地址。
     for(int i = 0; i < 256; i++)
         SETGATE(idt[i], /*not trap*/0, GD_KTEXT, __vectors[i], DPL_KERNEL);
     SETGATE(idt[T_SWITCH_TOK], 0, GD_KTEXT, __vectors[T_SWITCH_TOK], DPL_USER);

     // A better answer will be:
     // https://piazza.com/class/i5j09fnsl7k5x0?cid=125
//     for(int i = 0; i < 256; i++) {
//         if(i == T_SYSCALL) {
//             SETGATE(idt[i], /*not trap*/0, GD_KTEXT, __vectors[i], DPL_USER);
//         }
//         else if(i < IRQ_OFFSET) {
//             // System fault, trap and NMI are all handled as trap. IF flag is not cleared
//             // so maskable interruption is not disabled.
//             SETGATE(idt[i], /*trap*/1, GD_KTEXT, __vectors[i], DPL_KERNEL);
//         }
//         else {
//             SETGATE(idt[i], /*not trap*/0, GD_KTEXT, __vectors[i], DPL_KERNEL);
//         }
//     }

     // https://c9x.me/x86/html/file_module_x86_id_156.html
     // &idt_pd  CANNOT be 6 bytes. How does this work??
     // How 6 bytes are constructed? asm volatile ("lidt (%0)" :: "r" (pd));
     // "r" => input oprand; lidt (%0) (use the data at address %0)
    // Why static inline??
     lidt(&idt_pd);
}

static const char *
trapname(int trapno) {
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(const char * const)) {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

/* trap_in_kernel - test if trap happened in kernel */
bool
trap_in_kernel(struct trapframe *tf) {
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  fs   0x----%04x\n", tf->tf_fs);
    cprintf("  gs   0x----%04x\n", tf->tf_gs);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err  0x%08x\n", tf->tf_err);
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL) {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

    if (!trap_in_kernel(tf)) {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void
print_regs(struct pushregs *regs) {
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}
// https://piazza.com/class/i5j09fnsl7k5x0?cid=122 Good
/* trap_dispatch - dispatch based on what type of trap occurred */
static void
trap_dispatch(struct trapframe *tf) {
    char c;

    switch (tf->tf_trapno) {
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        /* (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
         * (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
         * (3) Too Simple? Yes, I think so!
         */
        ticks ++;
        if(ticks % TICK_NUM == 0)
            print_ticks();
        break;
    case IRQ_OFFSET + IRQ_COM1:
        c = cons_getc();
        cprintf("serial [%03d] %c\n", c, c);
        break;
    case IRQ_OFFSET + IRQ_KBD:
        c = cons_getc();
        if( c == '3' || c == '0' || c == 'p')
            print_trapframe(tf);
        cprintf("kbd [%03d] %c\n", c, c);
        // TODO: this doesn't work. Need to figure out why???
        // TODO: Chalenge 2
        if(c == '3' && trap_in_kernel(tf))
            lab1_switch_to_user();
        if(c == '0' && !trap_in_kernel(tf))
            lab1_switch_to_kernel();
        break;
    //LAB1 CHALLENGE 1 : YOUR CODE you should modify below codes.
    // Follow this https://github.com/twd2/ucore_os_lab/blob/master/labcodes/lab1/kern/trap/trap.c
    case T_SWITCH_TOU:
        // Currently in Kernel mode. No need to switch ring in this block.
        // 当CPU执行这个指令时，由于是在switch_to_user执行在内核态，所以不存在特权级切换问题，
        // 硬件只会在内核栈中压入Error Code（可选）、EIP、CS和EFLAGS
        if (tf->tf_cs != USER_CS) {
            //当前在内核态，需要建立切换到用户态所需的trapframe结构的数据switchk2u
            //设置临时栈，指向switchk2u，这样iret返回时，CPU会从switchk2u恢复数据，而不是从现有栈恢复数据。
            // 我猜测这是因为在进入中断时，是内核态进入内核态，因此 CPU 没有压入 esp 与 ss，
            // 但在退出中断时，将从内核态返回用户态，原本的 trapframe 里并不包含 esp 与 ss，所以建立了一个临时的 trapframe

            struct trapframe switchk2u = *tf; // a shallow copy

            // iret doesn't change any of the data segments, so you will need to change them manually.
            switchk2u.tf_ds = switchk2u.tf_es = switchk2u.tf_gs = switchk2u.tf_fs = USER_DS;
            switchk2u.tf_cs = USER_CS;

            // TODO: how to understand this? This will be esp in user mode. Why it's using the same addr as in kernel mode? Why this equation??
            // But this should be just to set the esp inside user mode. based on the memory of kernel mode. //TODO: why can it be used by both??
            switchk2u.tf_esp = (uint32_t) tf + sizeof(struct trapframe); // this will be %esp after going to user mode.
            switchk2u.tf_ss = USER_DS;

            //设置EFLAG的I/O特权位，使得在用户态可使用in/out指令
            switchk2u.tf_eflags |= (3 << 12);

            // option 1
            // 在处理向用户态切换的部分中，将 (uint32_t *)tf - 1 的内容换成了 switchk2u 的地址，
            // TODO: why one addr before tf will be used by iret? Why could iret detect the &switchk2u address?
            // B/C originally pushed esp will be popped out, (uint32_t *)tf - 1 is the one at the top of stack. Correct??
//            *((uint32_t *)tf - 1) = (uint32_t)&switchk2u;

            // option 2
            // How iret restore the env for user mode? https://stackoverflow.com/questions/6892421/switching-to-user-mode-using-iret
            asm volatile (
            "movl %0, %%esp \n" // change stack.
            "jmp __trapret"
            :
            : "g"(&switchk2u) // https://gcc.gnu.org/onlinedocs/gcc/Simple-Constraints.html#Simple-Constraints
            );

            /* // Another implementation TODO: why would this work?
            tf->tf_gs = tf->tf_fs = tf->tf_es = tf->tf_ds = USER_DS;
            tf->tf_cs = USER_CS;
            tf->tf_esp = ((uintptr_t)tf) + sizeof(struct trapframe);
            tf->tf_ss = USER_DS;
            tf->tf_eflags |= 0x3000; // IOPL=3
            break;
            */
        }
        break;
    case T_SWITCH_TOK:
        // TODO: using gdb to debug. Seems switchu2k addr is not changing after assignment of Line 258???.
        if (tf->tf_cs != KERNEL_CS) {
            struct trapframe *switchu2k = (struct trapframe *) (tf->tf_esp -
                                                                sizeof(struct trapframe));// TODO:why? Check Line 221?
            for (int i = 0; i < sizeof(struct trapframe) / sizeof(uint32_t); i++) // not copy last 2 * 4bytes. (esp ss)
                ((uint32_t *) switchu2k)[i] = ((uint32_t *) tf)[i];
            switchu2k->tf_gs = switchu2k->tf_fs = switchu2k->tf_es = switchu2k->tf_ds = KERNEL_DS;
            switchu2k->tf_cs = KERNEL_CS;
            switchu2k->tf_ss = KERNEL_DS;
            switchu2k->tf_eflags &= ~(3 << 12);
            asm volatile (
            "movl %0, %%esp \n" // change stack.
            "jmp __trapret"
            :
            : "g"(switchu2k) // https://gcc.gnu.org/onlinedocs/gcc/Simple-Constraints.html#Simple-Constraints
            );
        }
        break;
        /*{
        // now we are using kernel stack (stack0).
        struct trapframe *new_tf = tf->tf_esp - (sizeof(struct trapframe) - 2 * sizeof(uint32_t));
        // copy
        for (int i = 0; i < sizeof(struct trapframe) / sizeof(uint32_t) - 2; ++i) {
            ((uint32_t *)new_tf)[i] = ((uint32_t *)tf)[i];
        }
        new_tf->tf_gs = new_tf->tf_fs = new_tf->tf_es = new_tf->tf_ds = KERNEL_DS;
        new_tf->tf_cs = KERNEL_CS;
        new_tf->tf_eflags &= ~0x3000; // IOPL=0
        asm volatile ("movl %0, %%esp\n" // change stack
                      "jmp __trapret"
                      :
                      : "g"(new_tf));
        break;
    }*/
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        // in kernel, it must be a mistake
        if ((tf->tf_cs & 3) == 0) {
            print_trapframe(tf);
            panic("unexpected trap in kernel.\n");
        }
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void
trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}

