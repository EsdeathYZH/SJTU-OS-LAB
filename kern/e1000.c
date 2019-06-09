#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

static struct E1000 *base;

struct tx_desc *tx_descs;
#define N_TXDESC (PGSIZE / sizeof(struct tx_desc))
struct rx_desc *rx_descs;
#define N_RXDESC (PGSIZE / sizeof(struct rx_desc))

//struct e1000_tx_desc __attribute__((aligned(128)))trans_desc_table[N_TXDESC] = { { 0 } };
char transmit_packet_buffer[N_TXDESC][MAX_PKT_SIZE];
char receive_packet_buffer[N_RXDESC][MAX_PKT_SIZE];

//volatile void* e1000;
// LAB 6: Your driver code here
static void set_rs_bit(struct tx_desc* ptr){
    //ptr->cmd |= E1000_TX_CMD_RS;
	ptr->cmd |= 8;
}

static void set_eop_bit(struct tx_desc* ptr){
	ptr->cmd |= E1000_TX_CMD_EOP;
}

static void set_length(struct tx_desc* ptr, uint32_t len){
    ptr->length = len;
}

static void set_dd_bit(struct tx_desc* ptr){
    ptr->status |= E1000_TX_STATUS_DD;
}

static void clear_dd_bit(struct tx_desc* ptr){
    ptr->status &= (~E1000_TX_STATUS_DD);
}

static bool check_dd_bit(struct tx_desc* ptr){
    return (ptr->status & 1);
}

static uint64_t read_tdt(){
    return base->TDT;
}

static void set_tdt(uint64_t tail){
    base->TDT = tail;
}

static uint64_t read_rdt(){
    return base->RDT;
}

static void set_rdt(uint64_t tail){
    base->RDT = tail;
}

int
e1000_tx_init()
{
	// Allocate one page for descriptors
	struct PageInfo* page =  page_alloc(1);
	tx_descs = page2kva(page);
	
	// Initialize all descriptors
	for(int i = 0; i < N_TXDESC; i++){
		tx_descs[i].addr = PADDR(transmit_packet_buffer[i]);
		set_dd_bit(&tx_descs[i]);
	}

	// Set hardware registers
	// Look kern/e1000.h to find useful definations
	base->TDBAL = (uint32_t)PADDR(tx_descs);
	base->TDBAH = 0;
	base->TDLEN = N_TXDESC * sizeof(struct tx_desc);
	base->TDH = 0;
	base->TDT = 0;
	//TCTL.EN
	base->TCTL |= E1000_TCTL_EN;
	//TCTL.PSP
	base->TCTL |= E1000_TCTL_PSP;
	//base->TCTL &= ~E1000_TCTL_CT;
	//TCTL.CT
	base->TCTL |= E1000_TCTL_CT_ETHER;
	//base->TCTL &= ~E1000_TCTL_COLD;
	//TCTL.COLD
	base->TCTL |= E1000_TCTL_COLD_FULL_DUPLEX;
	//TIPG
	base->TIPG = E1000_TIPG_DEFAULT;
	return 0;
}

int
e1000_rx_init()
{
	// Allocate one page for descriptors
	struct PageInfo* page =  page_alloc(1);
	rx_descs = page2kva(page);

	// Initialize all descriptors
	// You should allocate some pages as receive buffer
	for(int i = 0; i < N_RXDESC; i++){
		rx_descs[i].addr = PADDR(&receive_packet_buffer[i]);
	}

	// Set hardward registers
	// Look kern/e1000.h to find useful definations
	base->RAL = 0x12005452;
	base->RAH = 0x5634;
	base->RDBAL = (uint32_t)PADDR(rx_descs);
	base->RDBAH = 0;
	base->RDLEN = N_RXDESC * sizeof(struct rx_desc);
	base->RDH = 1;
	base->RDT = 0;
	base->RCTL |= E1000_RCTL_EN;
	base->RCTL |= E1000_RCTL_BAM;
	base->RCTL |= E1000_RCTL_BSIZE_2048;
	base->RCTL |= E1000_RCTL_SECRC;
	return 0;
}

int
pci_e1000_attach(struct pci_func *pcif)
{
	// Enable PCI function
	// Map MMIO region and save the address in 'base;
	pci_func_enable(pcif);
	base = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	e1000_tx_init();
	e1000_rx_init();
	return 0;
}

int
e1000_tx(const void *buf, uint32_t len)
{
	// Send 'len' bytes in 'buf' to ethernet
	// Hint: buf is a kernel virtual address
	uint64_t tx_tail = read_tdt();
	if(len > MAX_PKT_SIZE){
		cprintf("dsafsfda\n");
		return -E_INVAL;
	}
	if(!check_dd_bit(&tx_descs[tx_tail])){
		return -E_AGAIN;
	}
	cprintf("tail index:%d\n", tx_tail);
	memset(transmit_packet_buffer[tx_tail], 0, MAX_PKT_SIZE);
	memmove(transmit_packet_buffer[tx_tail], buf, len);
	set_length(&tx_descs[tx_tail], len);
	set_rs_bit(&tx_descs[tx_tail]);
	set_eop_bit(&tx_descs[tx_tail]);
	clear_dd_bit(&tx_descs[tx_tail]);
	tx_tail = (tx_tail+1) % N_TXDESC;
	set_tdt(tx_tail);
	return 0;
}

int
e1000_rx(void *buf, uint32_t len)
{
	// Copy one received buffer to buf
	// You could return -E_AGAIN if there is no packet
	// Check whether the buf is large enough to hold
	// the packet
	int length;
	uint64_t rx_tail = (read_rdt()+1) % N_RXDESC;
	if((rx_descs[rx_tail].status & E1000_RX_STATUS_DD)==0){
		return -E_AGAIN;
	}
	// Do not forget to reset the decscriptor and
	// give it back to hardware by modifying RDT
	//memset(receive_packet_buffer[rx_head], 0, MAX_PKT_SIZE);
	cprintf("length:%d\n", rx_descs[rx_tail].length);
	cprintf("addr:%x\n", &receive_packet_buffer[rx_tail]);
	cprintf("buf addr:%x\n", (uint32_t)buf);
	length = rx_descs[rx_tail].length;
	memmove(buf,receive_packet_buffer[rx_tail], rx_descs[rx_tail].length);
	rx_descs[rx_tail].status &= (~E1000_RX_STATUS_DD);
	rx_descs[rx_tail].status &= (~E1000_RX_STATUS_EOP);
	set_rdt(rx_tail);
	return length;
}
