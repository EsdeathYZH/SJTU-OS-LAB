// test register restore on user-level page fault return

#include <inc/lib.h>
int zero;

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

static void
exception(struct UTrapframe *utf)
{
	int r;
    cprintf("Catch a processor exception:%s!!!\n", excnames[utf->utf_trapno]);
    exit();
}

void
umain(int argc, char **argv)
{
	set_exception_handler(exception);
    zero = 0;
    cprintf("1/0 is %08x!\n", 1/zero);
	return;
}
