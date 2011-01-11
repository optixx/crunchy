/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License (Version 2,
 *  June 1991) as published by the Free Software Foundation.  At the
 *  time of writing, that license was published by the FSF with the URL
 *  http://www.gnu.org/copyleft/gpl.html, and is incorporated herein by
 *  reference.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef JTAG_UTIL_H
#define JTAG_UTIL_H

int initialize_jtag(void);
void reset_jtag(void);
int jtag_issue_inst(unsigned char nbits, unsigned int newinst);
void run_test_idle();

void enter_ir_state(unsigned char pad);
void leave_ir_state(unsigned char pad);
void enter_dr_state(unsigned char pad);
void leave_dr_state(unsigned char pad);
unsigned char clock_inout_byte(unsigned char bits);
unsigned char clock_inout_byte_msb(unsigned char bits);
void clock_out_bytes_msb(xdata unsigned char *p, unsigned char bits);
unsigned char clock_inout_bits(unsigned char value);

void jtag_user1_setup (void);
unsigned char jtag_user1_data (unsigned char inp);
void jtag_user1_finish (void);

#endif
