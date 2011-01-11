/* -*- c++ -*- */
/*
 * Copyright 2003,2004 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.	If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef INCLUDED_FPGA_REGS_COMMON_H
#define INCLUDED_FPGA_REGS_COMMON_H

// This file defines registers common to all FPGA configurations.
// Registers 0 to 31 are reserved for use in this file.


// The FPGA needs to know the rate that samples are coming from and
// going to the A/D's and D/A's.  div = 128e6 / sample_rate

#define	FR_TX_SAMPLE_RATE_DIV	 0
#define	FR_RX_SAMPLE_RATE_DIV	 1

// offset corrections for ADC's and DAC's (2's complement)

#define	FR_ADC_OFFSET_3_2	 2	// hi 16: ADC3, lo 16: ADC2
#define	FR_ADC_OFFSET_1_0	 3

#define	FR_MASTER_CTRL		 4	// master enable and reset controls
#  define  bmFR_MC_ENABLE_TX		(1 << 0)
#  define  bmFR_MC_ENABLE_RX		(1 << 1)
#  define  bmFR_MC_RESET_TX		(1 << 2)
#  define  bmFR_MC_RESET_RX		(1 << 3)

// i/o direction registers for pins that go to daughterboards.
// Setting the bit makes it an output from the FPGA to the d'board.
// top 16 is mask, low 16 is value

#define	FR_OE_0			 5	// slot 0
#define	FR_OE_1			 6
#define	FR_OE_2			 7
#define	FR_OE_3			 8

// i/o registers for pins that go to daughterboards.
// top 16 is a mask, low 16 is value

#define	FR_IO_0			 9	// slot 0
#define	FR_IO_1			10
#define	FR_IO_2			11
#define	FR_IO_3			12

#define	FR_MODE			13
#  define  bmFR_MODE_NORMAL		      0
#  define  bmFR_MODE_LOOPBACK		(1 << 0)	// enable digital loopback
#  define  bmFR_MODE_RX_COUNTING	(1 << 1)	// Rx is counting

#endif /* INCLUDED_FPGA_REGS_COMMON_H */
