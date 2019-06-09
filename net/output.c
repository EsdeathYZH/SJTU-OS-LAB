#include "ns.h"

extern union Nsipc nsipcbuf;

void
sleep(int sec)
{
	unsigned now = sys_time_msec();
	unsigned end = now + sec * 1000;

	if ((int)now < 0 && (int)now > -MAXERROR)
		panic("sys_time_msec: %e", (int)now);
	if (end < now)
		panic("sleep: wrap");

	while (sys_time_msec() < end)
		sys_yield();
}

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet request (using ipc_recv)
	//	- send the packet to the device driver (using sys_net_send)
	//	do the above things in a loop
	while(1){
		envid_t env;
		int r;
		if((r = ipc_recv(&env, &nsipcbuf, NULL)) < 0){
			panic("ipc_recv:%d", r);
		}
		if(env != ns_envid) continue;
		r = sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		while(r == -E_AGAIN){
			//sleep(2);
			r = sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		}
		if(r < 0){
			panic("sys_net_send:%d", r);
		}
	}
}
