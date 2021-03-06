/* 
 * USRP - Universal Software Radio Peripheral
 *
 * Copyright (C) 2003 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef INCLUDED_FPGA_LOAD_H
#define INCLUDED_FPGA_LOAD_H

unsigned char fpga_load_begin (void);
unsigned char fpga_load_xfer (xdata unsigned char *p, unsigned char len);
unsigned char fpga_rxtx(unsigned char value);
unsigned char fpga_tx(xdata unsigned char *p);
unsigned char fpga_load_end (void);
unsigned char fpga_load_end_shutdown (void);

#endif /* INCLUDED_FPGA_LOAD_H */
