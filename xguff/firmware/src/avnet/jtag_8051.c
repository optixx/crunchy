/* SDCC 8051 implementation of JTAG functionality
 *
 * in-testing for Avnet Virtex 4LX Evaluation board
 * with CY7C68013 8051-derivative chip.
 * Among other things, the following include file needs
 * to define JTAG_TCK, JTAG_TMS, JTAG_TDI, and JTAG_TDO
 * as special register bits.
 * These assignments are, in general, board-specific.
 */
#include "jtag_util.h"
#include "fpga_pins.h"

/* A "nop" is inserted between JTAG_TCK = 1 and JTAG_TCK = 0 in the
 * following macro, to keep the HIGH time of every JTAG_TCK pulse
 * exactly 250 ns.  The "nop" slot is filled with "rrc" in the
 * time-critical clk_inout_byte routine.  The smallest JTAG_CLK LOW
 * time is 330 ns, 500 ns in clk_inout_byte().
 */
#define jtag_clock() do { JTAG_TCK = 1; _asm nop _endasm; JTAG_TCK = 0;} while (0)

/* Transfers bytes to and from the JTAG chain LSB first.
 * Combined with a simple loop written in C (see i2c.c), this routine
 * moves one byte in 12.2 us. */
unsigned char
clock_inout_byte (unsigned char bits) /* 13 code bytes */
{
	(void) bits; /* only used in assembly */
    _asm
	mov     a, dpl
	mov     r2, #8

cib_loop:

	setb    _JTAG_TCK
	rrc     a
	clr     _JTAG_TCK
	
	mov     _JTAG_TDI, c
	mov     c, _JTAG_TDO

	djnz    r2, cib_loop

	rrc     a
	mov     dpl, a
    _endasm;
}

unsigned char
clock_inout_byte_msb (unsigned char bits) /* 13 code bytes */
{
	(void) bits; /* only used in assembly */
    _asm
	mov     a, dpl
	mov     r2, #8

cib_loop_msb:
	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c
	mov     c, _JTAG_TDO
	
	djnz    r2, cib_loop_msb

	rlc     a
	mov     dpl, a
    _endasm;
}

/*
 * Send TMS/TDI pair out, record the response
 */
unsigned char clock_inout_bits(unsigned char value)
{
	unsigned char ret;

	JTAG_TMS = (value >> 1) & 1;
	JTAG_TDI = value & 1;
	JTAG_TCK = 1;
	ret = JTAG_TDO;
	JTAG_TCK = 0;

	return ret;
}

/* Transfers bytes to the JTAG chain MSB first.
 * One bit time is 583 ns (7 cycles at 12 MHz)
 * This routine moves one byte in 5.32 us (667 ns overhead/byte, 8 cycles?).
 * So 64 bytes move in 342 us, add 158 us for USB EP0 transfer overhead
 * (are we limited to 2 kHz by USB design?)
 * should move 977488 bytes in 7.64 seconds: verified */
void
clock_out_bytes_msb (xdata unsigned char *p, unsigned char bits) /* 63 code bytes */
{
	(void) p; /* only used in assembly */
	(void) bits; /* only used in assembly */
    _asm
	mov     r2, _clock_out_bytes_msb_PARM_2
cobm_loop:
	movx    a, @dptr
	rlc     a
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	rlc     a
	clr     _JTAG_TCK
	mov     _JTAG_TDI, c

	setb    _JTAG_TCK
	inc     dptr
	clr     _JTAG_TCK

	djnz    r2, cobm_loop
    _endasm;
}


/***-------------------------------------***/
/***  Reset the JTAG TAP controller      ***/
/***-------------------------------------***/
/* NOTE: after reset, Run-Test/Idle state will be entered */
void reset_jtag(void) /* 25 code bytes */
{
	unsigned char i;
	for (i=0; i<5; i++) {
		JTAG_TMS = 1; jtag_clock();   /* 5 cycles with TMS=1 */
	}
	JTAG_TMS = 0; jtag_clock();   /* TMS=0 to enter run-test/idle */
}

void enter_ir_state(unsigned char pad) /* 11 code bytes */
{
	JTAG_TMS = 1; jtag_clock();   /* goto Select-DR-Scan */
#if 1
	enter_dr_state(pad);
#else
	JTAG_TMS = 1; jtag_clock();   /* goto Select-IR-Scan */
	JTAG_TMS = 0; jtag_clock();   /* goto Capture-IR     */
	/* let leading clock in data transfer move us to Shift-IR */
	while (pad) {
		jtag_clock();
		JTAG_TDI = 1; /* pad with 1s to form BYPASS instruction */
		pad--;
	}
#endif
	return;
}

void leave_ir_state(unsigned char pad) /* 4 code bytes */
{
#if 1
	leave_dr_state(pad);
#else
	while (pad) {
		jtag_clock();
		JTAG_TDI = 1; /* pad with 1s to form BYPASS instruction */
		pad--;
	}

	JTAG_TMS = 1; jtag_clock();   /* goto Exit1-IR       */
	JTAG_TMS = 1; jtag_clock();   /* goto Update-IR      */
#endif
	return;
}

/* Also works for enter_ir_state, if you
 *   JTAG_TMS = 1; jtag_clock();
 * first.  In that case it's essential that the padding use 1's */
void enter_dr_state(unsigned char pad) /* 30 code bytes */
{
	JTAG_TMS = 1; jtag_clock();   /* goto Select-DR-Scan */
	JTAG_TMS = 0; jtag_clock();   /* goto Capture-DR     */
	/* let leading clock in data transfer move us to Shift-DR */
	while (pad) {
		jtag_clock();
		JTAG_TDI = 1; /* fill bypass register bits with 1s */
		pad--;
	}
	return;
}

/* Except for the comments and the fill pattern, this is exactly
 * the same as leave_ir_state().
 * Should eventually combine them to reduce binary code size.
 */
void leave_dr_state(unsigned char pad) /* 30 code bytes */
{
	while (pad) {
		jtag_clock();
		JTAG_TDI = 1; /* fill bypass register bits with 1s */
		pad--;
	}
	JTAG_TMS = 1; jtag_clock();   /* goto Exit1-DR       */
	JTAG_TMS = 1; jtag_clock();   /* goto Update-DR      */
}

/***-------------------------------------***/
/***  Enter a new instr. in TAP machine  ***/
/***-------------------------------------***/

/* Assumes TAP is on Run-Test/Idle or Update-DR/Update-IR states at entry */
/* On exit, stays at Update-IR  */
/* Maximum bit length is 16 (unsigned int on 8051) */
/* Some comments might be stale, I took out the
 * enter_ir_state/leave_ir_state calls as of 2006-03-21. */

int jtag_issue_inst(unsigned char nbits, unsigned int newinst) /* 56 code bytes */
{
	/* printf("sending %d bits of %d to IR\n",nbits,newinst); */
	while (nbits) {
		jtag_clock();
		JTAG_TDI = newinst&1;
		newinst >>= 1;
		nbits--;
	}

	return 0;
}

/* Run-Test/Idle */
void run_test_idle() /* 10 code bytes */
{
	JTAG_TMS=0;
	JTAG_TDI=0;
	jtag_clock();
}

int initialize_jtag(void) /* 22 code bytes */
{
	JTAG_TMS = 0;
	JTAG_TDI = 0;
	JTAG_TCK = 0;
	JTAG_PORT_SETUP();
	reset_jtag();
	return 0;
}
