#include "fpga_reset.h"

/* see pin assignment comments in fpga_pins.h */
sbit at 0x80+4 FPGA_PROG;
sbit at 0x80+5 FPGA_INIT;

void
fpga_set_reset (unsigned char on)
{
	/* LLRF4 board, FPGA_PROG# directly connected to PA0
	 *              FPGA_INIT# directly connected to PA1 */
	if (on) {
		FPGA_PROG = 0;
		while ( FPGA_INIT == 1 ) { /* spin until low, observed instant */ }
		FPGA_PROG = 1;
		/* buggy code generated for (FPGA_INIT == 0) */
		while ( !(FPGA_INIT == 1) ) { /* spin until high, observed 200 us */ }
	} else {
		FPGA_PROG = 1;
	}
	return;
}
