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

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display information about the function backtrace", mon_backtrace},
	{ "time", "Display total time of a command runs", mon_time},
	{ "setpermission", "Explicitly set the permissions of any mapping", mon_set_permission},
	{ "dumpmemory", "Dump the contents of a range of memory given either a virtual or physical address range", mon_dump_memory},
	{ "showmappings", "Display in a useful and easy-to-read format all of the physical page mappings", mon_show_mappings}
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
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

int
mon_show_mappings(int argc, char **argv, struct Trapframe *tf)
{
	if(argc != 3){
		cprintf("command use: showmappings [start address] [end address]\n");
		return 0;
	}
	uintptr_t start_addr = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	uintptr_t end_addr = ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);

	cprintf("Memory mappings from %08x to %08x:\n", start_addr, end_addr);
	for(uintptr_t addr = start_addr; addr <= end_addr; addr += PGSIZE){
		//pte_t* pte_ptr = (pte_t*)((UVPT + PDX(addr)*PGSIZE)) + PTX(addr);
		pte_t* pte_ptr = pgdir_walk(kern_pgdir, (void*)addr, 0);
		if(!pte_ptr){
			cprintf("%08x -> %08x permisson bits: --/--\n", addr, 0);
			continue;
		}
		cprintf("%08x -> %08x permisson bits: ", addr, PTE_ADDR(*pte_ptr));
		if((*pte_ptr) & PTE_P){
			if(((*pte_ptr) & PTE_U) && ((*pte_ptr) & PTE_W)){
				cprintf("RW/RW\n");
			}else if((*pte_ptr) & PTE_U){
				cprintf("R-/R-\n");
			}else if((*pte_ptr) & PTE_W){
				cprintf("RW/--\n");
			}else{
				cprintf("R-/--\n");
			}
		}else{
			cprintf("--/--\n");
		}
	}
	return 0;
}

int
mon_dump_memory(int argc, char **argv, struct Trapframe *tf)
{
	if(argc != 4){
		cprintf("command use: dumpmemory [-v/-p] [start address] [end address]\n");
		return 0;
	}
	uintptr_t addr1 = strtol(argv[2], NULL, 16);
	uintptr_t addr2 = strtol(argv[3], NULL, 16);
	uintptr_t start_addr = ROUNDDOWN(addr1, PGSIZE);
	uintptr_t end_addr = ROUNDDOWN(addr2, PGSIZE);

	if(!strcmp("-p", argv[1])){
		start_addr = (uintptr_t)KADDR(start_addr);
		end_addr = (uintptr_t)KADDR(end_addr);
	}

	cprintf("Dump memory from %08x to %08x:\n", addr1, addr2);
	for(uintptr_t addr = start_addr; addr <= end_addr; addr += PGSIZE){
		uintptr_t start = MAX(addr1, addr);
		uintptr_t end = MIN(addr2, addr+PGSIZE);
		pte_t* pte_ptr = pgdir_walk(kern_pgdir, (void*)addr, 0);
		if(!pte_ptr || !((*pte_ptr) | PTE_P)){
			cprintf("Invalid mapping %08x\n", addr, 0);
			continue;
		}
		long next_line = 1;
		for(unsigned char* index = (unsigned char*)start; index < (unsigned char*)end; index++,next_line++){
			cprintf("0x%02x ", (*index));
			if(next_line%8 == 0) cprintf("\n");
		}
	}
	return 0;
}

int mon_set_permission(int argc, char **argv, struct Trapframe *tf)
{
	if(argc != 3){
		cprintf("command use: setpermission [address] [permission bit]\n");
		return 0;
	}
	
	uintptr_t addr = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	uintptr_t perm_bit = strtol(argv[2], NULL, 16);
	if(perm_bit >= 512){
		cprintf("Invalid permission bits!!\n");
		return 0;
	}
	cprintf("Set permission at %08x as %08x:\n", addr, perm_bit);
	pte_t* pte_ptr = pgdir_walk(kern_pgdir, (void*)addr, 0);
	if(!pte_ptr){
		cprintf("Invalid mapping!!\n");
		return 0;
	}
	*pte_ptr = ((*pte_ptr) & (~511)) | perm_bit;
	return 0;
}

uint64_t rdtsc()
{
        uint32_t low,high;

        __asm__ __volatile__
        (
         "rdtsc":"=a"(low),"=d"(high)
        );
        return (uint64_t)high<<32|low;
}

int mon_time(int argc, char **argv, struct Trapframe *tf)
{
	int cmd_index = -1;
	for (int i = 0; i < ARRAY_SIZE(commands); i++){
		if (strcmp(argv[1], commands[i].name) == 0) cmd_index = i;
	}
	if(cmd_index == -1){
		cprintf("command not found!!!\n");
		return 0;	
	}
	uint64_t start_time = rdtsc();
	commands[cmd_index].func(argc-1, argv+1, tf);
	uint64_t end_time = rdtsc();
	cprintf("kerninfo cycles: %ld\n", end_time - start_time);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    memset(str, 0x41, sizeof(str) - 1);
    // Your code here.
    char *pret_addr = (char*) read_pretaddr();
    char *pret_addr1 = pret_addr + 1;
    char *pret_addr2 = pret_addr + 2;
    char *pret_addr3 = pret_addr + 3;

    uint32_t overflowfunc_addr = (uint32_t) do_overflow;
	uint32_t normal_addr = *((uint32_t*)pret_addr);
    uint32_t num1 = overflowfunc_addr % 0x100;
    uint32_t num2 = (overflowfunc_addr >> 8) % 0x100;
    uint32_t num3 = (overflowfunc_addr >> 16) % 0x100;
    uint32_t num4 = (overflowfunc_addr >> 24) % 0x100;
	uint32_t n_num1 = normal_addr % 0x100;
    uint32_t n_num2 = (normal_addr >> 8) % 0x100;
    uint32_t n_num3 = (normal_addr >> 16) % 0x100;
    uint32_t n_num4 = (normal_addr >> 24) % 0x100;
	//do_overflow();
    str[num1] = 0;
	cprintf("%s%n", str, pret_addr); 
	str[num1] = 0xd;
	str[255] = 0;
	str[num2] = 0;
	cprintf("%s%n", str, pret_addr1);
	str[num2] = 0xd;
	str[255] = 0;
	str[num3] = 0;
	cprintf("%s%n", str, pret_addr2);
	str[num3] = 0xd;
	str[255] = 0;
	str[num4] = 0;
	cprintf("%s%n", str, pret_addr3);
	str[num4] = 0xd;
	str[255] = 0;
	//return normal address;
    str[n_num1] = 0;
	cprintf("%s%n", str, pret_addr+4); 
	str[n_num1] = 0xd;
	str[255] = 0;
	str[n_num2] = 0;
	cprintf("%s%n", str, pret_addr1+4);
	str[n_num2] = 0xd;
	str[255] = 0;
	str[n_num3] = 0;
	cprintf("%s%n", str, pret_addr2+4);
	str[n_num3] = 0xd;
	str[255] = 0;
	str[n_num4] = 0;
	cprintf("%s%n", str, pret_addr3+4);
	str[n_num4] = 0xd;
	str[255] = 0;
}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t* ebp = (uint32_t*) read_ebp();

	while(ebp != 0){
		struct Eipdebuginfo info;
		debuginfo_eip(*(ebp+1), &info);

		cprintf("  eip %08x  ebp %08x  args %08x %08x %08x %08x %08x\n", 
			*(ebp+1), ebp, *(ebp+2), *(ebp+3), *(ebp+4), *(ebp+5), *(ebp+6));
		cprintf("         %s:%d %.*s+%d\n", info.eip_file, info.eip_line, 				info.eip_fn_namelen, info.eip_fn_name, *(ebp+1) - info.eip_fn_addr);
		ebp = (uint32_t*) *ebp;
	}
	overflow_me();
    cprintf("Backtrace success\n");
	return 0;
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
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
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
