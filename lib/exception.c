// User-level exception handler support.
// Rather than register the C exception handler directly with the
// kernel as the exception handler, we register the assembly language
// wrapper in exceptentry.S, which in turns calls the registered C
// function.

#include <inc/lib.h>


// Assembly language exception entrypoint defined in lib/pfentry.S.
extern void _exception_upcall(void);

// Pointer to currently installed C-language exception handler.
void (*_exception_handler)(struct UTrapframe *utf);

//
// Set the exception handler function.
// If there isn't one yet, _exception_handler will be 0.
// The first time we register a handler, we need to
// allocate an exception stack (one page of memory with its top
// at UXSTACKTOP), and tell the kernel to call the assembly-language
// _exception_upcall routine when a exception occurs.
//
void
set_exception_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_exception_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		envid_t envid = sys_getenvid();
		sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_W|PTE_U|PTE_P);
		sys_env_set_exception_upcall(envid, _exception_upcall);
	}

	// Save handler pointer for assembly to call.
	_exception_handler = handler;
}