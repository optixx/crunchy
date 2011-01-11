#include "fpga_load.h"
#include "jtag_util.h"
#include "usrp_commands.h"

#define USER1     (15*64+2)
#define USER2     (15*64+3)
#define CFG_IN    (15*64+5)
#define JPROGRAM  (15*64+11)   /* from xc3sprog, later confirmed with Xilinx */
#define JSTART    (15*64+12)
#define JSHUTDOWN (15*64+13)
#define BYPASS    (15*64+63)

xdata at USRP_JTAG_CONFIG_ADDR unsigned char jtag_config[16];

#if 0
unsigned char jtag_chain_b0=0;  /* bypass bits before active device */
unsigned char jtag_chain_b1=0;  /* bypass bits  after active device */
unsigned char jtag_chain_i0=0;  /* IR bits before active device     */
unsigned char jtag_chain_i1=0;  /* IR bits after  active device     */
unsigned char jtag_ir_len=6;    /* IR length                        */
#else
#define jtag_chain_b0 jtag_config[0]
#define jtag_chain_b1 jtag_config[1]
#define jtag_chain_i0 jtag_config[2]
#define jtag_chain_i1 jtag_config[3]
#define jtag_ir_len   jtag_config[4]
#endif

void
jtag_inst(unsigned int inst)
{
	enter_ir_state(jtag_chain_i1);
	jtag_issue_inst(jtag_ir_len, inst);
	leave_ir_state(jtag_chain_i0);
}

void
twelve_cycles(void)
{
	unsigned int i;
	for (i=0; i<12; i++) run_test_idle();
}

unsigned char
fpga_load_begin (void)
{
	initialize_jtag();

	jtag_inst(JPROGRAM);
	jtag_inst(CFG_IN);
	enter_dr_state(jtag_chain_b1);
	return 1;
}

/*
 * Transfer block of bytes from packet to FPGA serial configuration port
 */
unsigned char
fpga_load_xfer (xdata unsigned char *p, unsigned char bytecount)
{
	// while (bytecount-- > 0) clock_out_byte (*p++);
	clock_out_bytes_msb(p, bytecount);
	return 1;
}

/*
 * Transfer TMS/TDI from a packet to JTAG
 */
unsigned char
fpga_tx(xdata unsigned char *p)
{
	clock_inout_bits(*p);
	return 1;
}

/*
 * Transfer TMS/TDI from a packet to JTAG, record TDO
 */
unsigned char
fpga_rxtx(unsigned char value)
{
	return clock_inout_bits(value);
}

#if 0
/*
 * check for successful load...
 */
unsigned char
fpga_load_end_work (unsigned int newinst)
{
	(void) newinst;  /* not used */
	return 0;
}
#endif

/*
 * end the load, but start the operation
 */
unsigned char
fpga_load_end (void)
{
	leave_dr_state(jtag_chain_b0);
	reset_jtag();
	jtag_inst(JSTART);
	twelve_cycles();
	jtag_inst(BYPASS);  /* interesting comment in xc3sprog */
	return 1;
}

/*
 * end shutdown sequence, prepare to accept GHIGH command and bitstream.
 */
unsigned char
fpga_load_end_shutdown (void)
{
	leave_dr_state(jtag_chain_b0);
	jtag_inst(JSHUTDOWN);
	twelve_cycles();
	jtag_inst(CFG_IN);
	enter_dr_state(jtag_chain_b1);
	return 1;
}

/* control over the JTAG port, uses the same chain configuration as above */
void
jtag_user1_setup (void)
{
	initialize_jtag();
	jtag_inst(USER1);
	enter_dr_state(jtag_chain_b1);
}

unsigned char
jtag_user1_data (unsigned char inp)
{
	return clock_inout_byte_msb (inp);
}

void
jtag_user1_finish (void)
{
	leave_dr_state(jtag_chain_b0);
}
