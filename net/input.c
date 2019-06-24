#include "ns.h"

extern union Nsipc nsipcbuf;

extern void sleep(int sec);

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server (using ipc_send with NSREQ_INPUT as value)
	//	do the above things in a loop
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	while(1){
		int r;
		uint32_t length;
		while((r = sys_page_alloc(0, &nsipcbuf, PTE_P|PTE_W|PTE_U)) < 0){
			cprintf("sys_page_alloc: %e", r);
		}
		cprintf("1\n");
		r = sys_net_recv(nsipcbuf.pkt.jp_data, 0);
		while(r == -E_AGAIN){
			//sleep(2);
			//cprintf("3\n");
			sys_yield();
			r = sys_net_recv(nsipcbuf.pkt.jp_data, 0);
		}
		cprintf("2\n");
		if(r < 0){
			panic("sys_net_recv:%d", r);
		}
     	nsipcbuf.pkt.jp_len = r;    // r must be positive
     	//memmove(nsipcbuf.pkt.jp_data, buffer, length);
		while(sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_W|PTE_U) < 0) ;
	}
}
