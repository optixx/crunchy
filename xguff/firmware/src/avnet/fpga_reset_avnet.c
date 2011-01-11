#include "fpga_reset.h"
#include "fx2regs.h"

void
fpga_set_reset (unsigned char on)
{
	if (on) {
		/* GPIFIDLECTL & 0x08 operates the CTL3 pin,
		 * attached to FPGA_PROG#
		 */
		GPIFIDLECTL = 0x00;
		/* GPIFREADYSTAT & 0x10 (RDY4?) is the FPGA_INIT# signal */
		while ( GPIFREADYSTAT & 0x10 ) { /* spin until low */ }
		GPIFIDLECTL = 0x08;
		while (~GPIFREADYSTAT & 0x10 ) { /* spin until high */ }
	} else {
		GPIFIDLECTL = 0x08;
	}
	return;
}
