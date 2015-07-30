// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(uvpd[PDX(addr)] & PTE_P))
		panic("pgfault error: pgdir not present 0%x", uvpd[PDX(addr)]);
	
	if((err & FEC_WR) != FEC_WR ||
	   !(uvpt[PGNUM(addr)] & PTE_COW))
		panic("pgfault error: %x not FEC_WR or PTE_COW", err);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	// LAB 4: Your code here.
	addr = (void *)ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, PFTEMP, PTE_W|PTE_U|PTE_P);
	if(r < 0)
		panic("pgfault: sys_page_alloc error:%r", r);
	
	memmove(PFTEMP, addr, PGSIZE);

	r = sys_page_map(0, PFTEMP, 0, addr, PTE_W|PTE_U|PTE_P);
	if(r < 0)
		panic("pgfault: sys_page_map error:%r", r);
	
	r = sys_page_unmap(0, PFTEMP);
	if(r < 0)
		panic("pgfault: sys_page_unmap error:%r", r);
	
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	void * addr = (void *)(pn*PGSIZE); 	
	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW))
	{
		r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P);
		if(r < 0)
			panic("duppage: 0 to %x sys_page_map error:%r", envid ,r);
		r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P);
		if(r < 0)
			panic("duppage: 0 to %x sys_page_map error:%r", 0, r);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
        set_pgfault_handler(pgfault);
	envid_t childenvid = sys_exofork();
	if(childenvid < 0)
		panic("fork: sys_exofork error: %e\n", childenvid);
	if(childenvid == 0)
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	} 
       
	uintptr_t va;
	for(va = 0; va < USTACKTOP; va+=PGSIZE)
	{
		if((uvpd[PDX(va)]   & PTE_P) && 
		   (uvpt[PGNUM(va)] & PTE_U) &&
		   (uvpt[PGNUM(va)] & PTE_P))
			duppage(childenvid, PGNUM(va));
	}
	
	r = sys_page_alloc(childenvid, (void *)(UXSTACKTOP-PGSIZE), PTE_U|PTE_W|PTE_P);
	if(r < 0)
		panic("[%x]fork: sys_page_alloc error:%r", childenvid, r);

	extern void _pgfault_upcall(); 
	r = sys_env_set_pgfault_upcall(childenvid, _pgfault_upcall);
	if(r < 0)
		panic("[%x]fork: sys_env_set_pgfault error:%r", childenvid, r);
	
	r = sys_env_set_status(childenvid, ENV_RUNNABLE);
	if(r < 0)
		panic("[%x]fork: sys_env_set_stauts error:%r", childenvid, r);

	return childenvid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
