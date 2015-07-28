/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts ###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
/* 00 */ TRAPHANDLER_NOEC(t_divide, T_DIVIDE)
/* 01 */ TRAPHANDLER_NOEC(t_debug, T_DEBUG)
/* 02 */ TRAPHANDLER_NOEC(t_nmi, T_NMI)
/* 03 */ TRAPHANDLER_NOEC(t_brkpt, T_BRKPT)
/* 04 */ TRAPHANDLER_NOEC(t_oflow, T_OFLOW)
/* 05 */ TRAPHANDLER_NOEC(t_bound, T_BOUND)
/* 06 */ TRAPHANDLER_NOEC(t_illop, T_ILLOP)
/* 07 */ TRAPHANDLER_NOEC(t_device, T_DEVICE)
/* 08 */ TRAPHANDLER(t_dblflt, T_DBLFLT)
/* 10 */ TRAPHANDLER(t_tss, T_TSS)
/* 11 */ TRAPHANDLER(t_segnp, T_SEGNP)
/* 12 */ TRAPHANDLER(t_stack, T_STACK)
/* 13 */ TRAPHANDLER(t_gpflt, T_GPFLT)
/* 14 */ TRAPHANDLER(t_pgflt, T_PGFLT)
/* 16 */ TRAPHANDLER_NOEC(t_fperr, T_FPERR)
/* 17 */ TRAPHANDLER(t_align, T_ALIGN)
/* 18 */ TRAPHANDLER_NOEC(t_mchk, T_MCHK)
/* 19 */ TRAPHANDLER_NOEC(t_simderr, T_SIMDERR)

/* 48 */ TRAPHANDLER_NOEC(t_syscall, T_SYSCALL)
/*
 * Lab 3: Your code here for _alltraps
 */

_alltraps:
	pushl %ds
	pushl %es
	pushal

	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	pushl %esp
	call trap
