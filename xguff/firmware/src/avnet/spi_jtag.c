/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "spi.h"
#include "jtag_util.h"

static void
setup_enables (unsigned char enables)
{
	(void) enables;
}

void
init_spi (void)
{
}

// returns non-zero if successful, else 0
unsigned char
spi_read (unsigned char header_hi, unsigned char header_lo,
	  unsigned char enables, unsigned char format,
	  xdata unsigned char *buf, unsigned char len)
{
    (void) header_hi;
    (void) header_lo;
    (void) enables;
    (void) format;
    (void) buf;
    (void) len;
    return 0;		// error, too many enables set
}

// returns non-zero if successful, else 0
unsigned char
spi_write (unsigned char header_hi, unsigned char header_lo,
	   unsigned char enables, unsigned char format,
	   const xdata unsigned char *buf, unsigned char len)
{
    (void) enables;   // not used
    jtag_user1_setup();
    if (format & SPI_FMT_LSB){            // order: LSB
      return 0;   // error, not implemented
    } else {                              // order: MSB
      switch (format & SPI_FMT_HDR_MASK){
      case SPI_FMT_HDR_0:
        break;
      case SPI_FMT_HDR_1:
        jtag_user1_data (header_lo);
	break;
      case SPI_FMT_HDR_2:
        jtag_user1_data (header_hi);
	jtag_user1_data (header_lo);
	break;
      default:
        return 0;         // error
      }
      while (len-- > 0) {
        jtag_user1_data (*buf++);
      }
    }
    jtag_user1_finish();
    return 1;		// success
}
