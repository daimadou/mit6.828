// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

int showmappings(int argc, char **argv, struct Trapframe *tf);
int setm(int argc, char **argv, struct Trapframe *tf);
int showvm(int argc, char **argv, struct Trapframe *tf);

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display function call frames",mon_backtrace},
	{ "showmappings", "showmappings", showmappings },
	{ "setm", "setm", setm },
	{ "showvm", "showvm", showvm },
	{ "continue", "continue running current program", mon_continue},
	{ "step", "single step current program", mon_singlestep},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

void  
dump_debuginfo_eip(uintptr_t eip)
{
	struct Eipdebuginfo info;
        int ret = debuginfo_eip(eip, &info);
	if(ret != 0)
	{
		cprintf("can't find the debuginfo eip:%08x\n", eip);
		return;
        }
	
	cprintf("%8s%s:%d: %.*s+%d\n", " ", 
		info.eip_file, 
		info.eip_line, 
		info.eip_fn_namelen,
		info.eip_fn_name, 
		(eip - info.eip_fn_addr));
	
}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t ebp = read_ebp();
	for(;ebp != 0; ebp = *((uint32_t *)ebp))
	{
		uint32_t eip = *((uint32_t *)(ebp + 4));
		cprintf("ebp %08x  eip %08x args", ebp, eip);
		uint32_t * args_addr = (uint32_t *)(ebp + 8);
		int i;
		for(i = 0; i < 5; i++)
		{
			cprintf(" %08x", args_addr[i]);
		}
		cprintf("\n");
		dump_debuginfo_eip(eip);
	}
	return 0;
}

int
mon_continue(int argc, char **argv, struct Trapframe *tf)
{
	// written by Tan
	if (curenv && curenv->env_status == ENV_RUNNING) {
		cprintf("continue: %%eip = 0x%08x\n", tf->tf_eip);
		env_run(curenv);
	} else {
		cprintf("error: program not running.\n");
		return 0;
	}
}

int
mon_singlestep(int argc, char **argv, struct Trapframe *tf)
{
	// written by Tan
	if (curenv && curenv->env_status == ENV_RUNNING) {
		cprintf("single step: %%eip = 0x%08x\n", tf->tf_eip);
		tf->tf_eflags |= FL_TF;
		env_run(curenv);

	} else {
		cprintf("error: program not running.\n");
		return 0;
	}
}




/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}



uint32_t xtoi(char* buf) {
	uint32_t res = 0;
	buf += 2; //0x...
	while (*buf) { 
		if (*buf >= 'a') *buf = *buf-'a'+'0'+10;//aha
		res = res*16 + *buf - '0';
		++buf;
	}
	return res;
}
void pprint(pte_t *pte) {
	cprintf("PTE_P: %x, PTE_W: %x, PTE_U: %x\n", 
		*pte&PTE_P, *pte&PTE_W, *pte&PTE_U);
}
int
showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc == 1) {
		cprintf("Usage: showmappings 0xbegin_addr 0xend_addr\n");
		return 0;
	}
	uint32_t begin = xtoi(argv[1]), end = xtoi(argv[2]);
	cprintf("begin: %x, end: %x\n", begin, end);
	for (; begin <= end; begin += PGSIZE) {
		pte_t *pte = pgdir_walk(kern_pgdir, (void *) begin, 1);	//create
		if (!pte) panic("boot_map_region panic, out of memory");
		if (*pte & PTE_P) {
			cprintf("page %x with ", begin);
			pprint(pte);
		} else cprintf("page not exist: %x\n", begin);
	}
	return 0;
}

int setm(int argc, char **argv, struct Trapframe *tf) {
	if (argc == 1) {
		cprintf("Usage: setm 0xaddr [0|1 :clear or set] [P|W|U]\n");
		return 0;
	}
	uint32_t addr = xtoi(argv[1]);
	pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, 1);
	cprintf("%x before setm: ", addr);
	pprint(pte);
	uint32_t perm = 0;
	if (argv[3][0] == 'P') perm = PTE_P;
	if (argv[3][0] == 'W') perm = PTE_W;
	if (argv[3][0] == 'U') perm = PTE_U;
	if (argv[2][0] == '0') 	//clear
		*pte = *pte & ~perm;
	else 	//set
		*pte = *pte | perm;
	cprintf("%x after  setm: ", addr);
	pprint(pte);
	return 0;
}

int showvm(int argc, char **argv, struct Trapframe *tf) {
	if (argc == 1) {
		cprintf("Usage: showvm 0xaddr 0xn\n");
		return 0;
	}
	void** addr = (void**) xtoi(argv[1]);
	uint32_t n = xtoi(argv[2]);
	int i;
	for (i = 0; i < n; ++i)
		cprintf("VM at %x is %x\n", addr+i, addr[i]);
	return 0;
}
