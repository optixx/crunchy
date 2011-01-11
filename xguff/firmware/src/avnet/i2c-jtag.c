#include "jtag_util.h"
#include "fx2regs.h"

// returns non-zero if successful, else 0
unsigned char
i2c_read (unsigned char addr, xdata unsigned char *buf, unsigned char len)
{
	unsigned char flush;
	if (len == 0)	// reading zero bytes always works
		return 1;

	initialize_jtag();
	if (addr == 0) {    // ID chain, DR fresh after reset
		enter_dr_state(0);
		flush = 0;
		while (len-- > 0) {
			*buf++ = clock_inout_byte(flush);
			if (len<8) flush=0xff;
		}
		leave_dr_state(0);
		run_test_idle();
	} else if (addr == 1) {   // Probe IR chain length
		enter_ir_state(0);
		flush = 0;
		while (len-- > 0) {
			*buf++ = clock_inout_byte(flush);
			if (len<8) flush=0xff;
		}
		leave_ir_state(0);
		run_test_idle();
	} else if (addr == 2) {   // Port C dump
		*buf++ = IOC;
		len--;
		if (len > 0) {
			*buf++ = GPIFREADYSTAT;
			len--;
		}
		while (len-- > 0) *buf++ = 0;
	} else {   // unknown address
		return 0;
	}
	return 1;
}

// returns non-zero if successful, else 0
unsigned char
i2c_write (unsigned char addr, xdata const unsigned char *buf, unsigned char len)
{ 
	/* placeholder */
	(void) addr; /* unused */
	(void) buf; /* unused */
	(void) len; /* unused */
	return 0;
}
