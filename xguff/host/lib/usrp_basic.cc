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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "usrp_basic.h"
#include "usrp_prims.h"
#include "usrp_interfaces.h"
#include "fpga_regs_common.h"
#include "fusb.h"
#include <usb.h>
#include <stdexcept>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#define NELEM(x) (sizeof (x) / sizeof (x[0]))

// These set the buffer size used for each end point using the fast
// usb interface.  The kernel ends up locking down this much memory.

static const int FUSB_BUFFER_SIZE = 2 * (1L << 20);	// 2 MB (was 8 MB)
static const int FUSB_BLOCK_SIZE = fusb_sysconfig::max_block_size();
static const int FUSB_NBLOCKS    = FUSB_BUFFER_SIZE / FUSB_BLOCK_SIZE;


static const double POLLING_INTERVAL = 0.1;	// seconds

////////////////////////////////////////////////////////////////

static struct usb_dev_handle *
open_rx_interface (struct usb_device *dev)
{
  struct usb_dev_handle *udh = usrp_open_rx_interface (dev);
  if (udh == 0){
    fprintf (stderr, "usrp_basic_rx: can't open rx interface\n");
    usb_strerror ();
  }
  return udh;
}

static struct usb_dev_handle *
open_tx_interface (struct usb_device *dev)
{
  struct usb_dev_handle *udh = usrp_open_tx_interface (dev);
  if (udh == 0){
    fprintf (stderr, "usrp_basic_tx: can't open tx interface\n");
    usb_strerror ();
  }
  return udh;
}


//////////////////////////////////////////////////////////////////
//
//			usrp_basic
//
////////////////////////////////////////////////////////////////


// Given:
//   CLKIN = 64 MHz
//   CLKSEL pin = high 
//
// These settings give us:
//   CLKOUT1 = CLKIN = 64 MHz
//   CLKOUT2 = CLKIN = 64 MHz
//   ADC is clocked at  64 MHz
//   DAC is clocked at 128 MHz

usrp_basic::usrp_basic (int which_board, 
			struct usb_dev_handle *
			open_interface (struct usb_device *dev))
  : d_udh (0),
    d_usb_data_rate (16000000),	// SWAG
    d_bytes_per_poll ((int) (POLLING_INTERVAL * d_usb_data_rate)),
    d_verbose (false)
{
  memset (d_fpga_shadows, 0, sizeof (d_fpga_shadows));

  usrp_one_time_init ();

  // FIXME allow subclasses to load own code
  // if (!usrp_load_standard_bits (which_board, false))
  //   throw std::runtime_error ("usrp_basic/usrp_load_standard_bits");

  struct usb_device *dev = usrp_find_device (which_board);
  if (dev == 0){
    fprintf (stderr, "usrp_basic: can't find usrp[%d]\n", which_board);
    throw std::runtime_error ("usrp_basic/usrp_find_device");
  }

  if (!(usrp_usrp1_p (dev) || usrp_usrp2_p (dev))){
    fprintf (stderr, "usrp_basic: sorry, this code only works with Rev 1 and 2 USRPs\n");
    throw std::runtime_error ("usrp_basic/bad_rev");
  }

  if ((d_udh = open_interface (dev)) == 0)
    throw std::runtime_error ("usrp_basic/open_interface");

  // initialize registers that are common to rx and tx

  _write_fpga_reg (FR_MODE, 0);		// ensure we're in normal mode
}

usrp_basic::~usrp_basic ()
{
  if (d_udh)
    usb_close (d_udh);
}

bool
usrp_basic::initialize ()
{
  return true;		// nop
}

bool
usrp_basic::start ()
{
  return true;		// nop
}

bool
usrp_basic::stop ()
{
  return true;		// nop
}

void
usrp_basic::set_usb_data_rate (int usb_data_rate)
{
  d_usb_data_rate = usb_data_rate;
  d_bytes_per_poll = (int) (usb_data_rate * POLLING_INTERVAL);
}

bool
usrp_basic::write_aux_dac (int slot, int which_dac, int value)
{
  return usrp_write_aux_dac (d_udh, slot, which_dac, value);
}

bool
usrp_basic::read_aux_adc (int slot, int which_adc, int *value)
{
  return usrp_read_aux_adc (d_udh, slot, which_adc, value);
}

int
usrp_basic::read_aux_adc (int slot, int which_adc)
{
  int	value;
  if (!read_aux_adc (slot, which_adc, &value))
    return READ_FAILED;

  return value;
}

bool
usrp_basic::write_i2c (int i2c_addr, const std::string buf)
{
  return usrp_i2c_write (d_udh, i2c_addr, buf.data (), buf.size ());
}

std::string
usrp_basic::read_i2c (int i2c_addr, int len)
{
  if (len <= 0)
    return "";

  char buf[len];

  if (!usrp_i2c_read (d_udh, i2c_addr, buf, len))
    return "";

  return std::string (buf, len);
}

// ----------------------------------------------------------------

bool
usrp_basic::set_adc_offset (int which, int offset)
{
  int	r, v;

  switch (which){
  case 0:
    r = FR_ADC_OFFSET_1_0;
    v = (d_fpga_shadows[r] & 0xffff0000) | (offset & 0xffff);
    break;
  case 1:
    r = FR_ADC_OFFSET_1_0;
    v = (d_fpga_shadows[r] & 0x0000ffff) | ((offset & 0xffff) << 16);
    break;
  case 2:
    r = FR_ADC_OFFSET_3_2;
    v = (d_fpga_shadows[r] & 0xffff0000) | (offset & 0xffff);
    break;
  case 3:
    r = FR_ADC_OFFSET_3_2;
    v = (d_fpga_shadows[r] & 0x0000ffff) | ((offset & 0xffff) << 16);
    break;
  default:
    return false;
  }
  return _write_fpga_reg (r, v);
}

// ----------------------------------------------------------------

bool
usrp_basic::_write_fpga_reg (int regno, int value)
{
  if (d_verbose){
    fprintf (stdout, "_write_fpga_reg (%3d, 0x%08x)\n", regno, value);
    fflush (stdout);
  }

  if (regno >= 0 && regno < MAX_REGS)
    d_fpga_shadows[regno] = value;

  return usrp_write_fpga_reg (d_udh, regno, value);
}

bool
usrp_basic::_read_fpga_reg (int regno, int *value)
{
  return usrp_read_fpga_reg (d_udh, regno, value);
}

int
usrp_basic::_read_fpga_reg (int regno)
{
  int value;
  if (!_read_fpga_reg (regno, &value))
    return READ_FAILED;
  return value;
}

bool
usrp_basic::_set_led (int which, bool on)
{
  return usrp_set_led (d_udh, which, on);
}

#if 0
bool
usrp_basic::_set_fpga_reset (bool on)
{
  return usrp_set_fpga_reset (d_udh, on);
}
#endif

////////////////////////////////////////////////////////////////
//
//			   usrp_basic_rx
//
////////////////////////////////////////////////////////////////


usrp_basic_rx::usrp_basic_rx (int which_board)
  : usrp_basic (which_board, open_rx_interface),
    d_devhandle (0), d_ephandle (0),
    d_bytes_seen (0), d_first_read (true),
    d_rx_enable (false)
{
  // initialize rx specific registers

  // Reset the rx path and leave it disabled.
  set_rx_enable (false);
  usrp_set_fpga_rx_reset (d_udh, true);
  usrp_set_fpga_rx_reset (d_udh, false);

  // probe_rx_slots (true);

  d_devhandle = fusb_sysconfig::make_devhandle (d_udh);
  d_ephandle = d_devhandle->make_ephandle (USRP_RX_ENDPOINT, true,
					   FUSB_BLOCK_SIZE, FUSB_NBLOCKS);
}

usrp_basic_rx::~usrp_basic_rx ()
{
  if (!set_rx_enable (false)){
    fprintf (stderr, "usrp_basic_rx: set_fpga_rx_enable failed\n");
    usb_strerror ();
  }

  d_ephandle->stop ();
  delete d_ephandle;
  delete d_devhandle;

}

bool
usrp_basic_rx::initialize ()
{
  if (!usrp_basic::initialize ())
    return false;
  return true;
}


bool
usrp_basic_rx::start ()
{
  if (!usrp_basic::start ())	// invoke parent's method
    return false;

  // fire off reads before asserting rx_enable

  if (!d_ephandle->start ()){
    fprintf (stderr, "usrp_basic_rx: failed to start end point streaming");
    usb_strerror ();
    return false;
  }

  if (!set_rx_enable (true)){
    fprintf (stderr, "usrp_basic_rx: set_rx_enable failed\n");
    usb_strerror ();
    return false;
  }
  
  return true;
}

bool
usrp_basic_rx::stop ()
{
  bool ok = usrp_basic::stop();

  if (!d_ephandle->stop()){
    fprintf (stderr, "usrp_basic_rx: failed to stop end point streaming");
    usb_strerror ();
    ok = false;
  }
  if (!set_rx_enable(false)){
    fprintf (stderr, "usrp_basic_rx: set_rx_enable(false) failed\n");
    usb_strerror ();
    ok = false;
  }
  return false;
}

usrp_basic_rx *
usrp_basic_rx::make (int which_board)
{
  usrp_basic_rx *u = 0;
  
  try {
    u = new usrp_basic_rx (which_board);
    if (!u->initialize ()){
      fprintf (stderr, "usrp_basic_rx::make failed to initialize\n");
      throw std::runtime_error ("usrp_basic_rx::make");
    }
    return u;
  }
  catch (...){
    delete u;
    return 0;
  }

  return u;
}

/*
 * \brief read data from the D/A's via the FPGA.
 * \p len must be a multiple of 512 bytes.
 *
 * \returns the number of bytes read, or -1 on error.
 *
 * If overrun is non-NULL it will be set true iff an RX overrun is detected.
 */
int
usrp_basic_rx::read (void *buf, int len, bool *overrun)
{
  int	r;
  
  if (overrun)
    *overrun = false;
  
  if (len < 0 || (len % 512) != 0){
    fprintf (stderr, "usrp_basic_rx::read: invalid length = %d\n", len);
    return -1;
  }

  r = d_ephandle->read (buf, len);
  if (r > 0)
    d_bytes_seen += r;

  /*
   * In many cases, the FPGA reports an rx overrun right after we
   * enable the Rx path.  If this is our first read, check for the
   * overrun to clear the condition, then ignore the result.
   */
  if (0 && d_first_read){	// FIXME
    d_first_read = false;
    bool bogus_overrun;
    usrp_check_rx_overrun (d_udh, &bogus_overrun);
  }

  if (overrun != 0 && d_bytes_seen >= d_bytes_per_poll){
    d_bytes_seen = 0;
    if (!usrp_check_rx_overrun (d_udh, overrun)){
      fprintf (stderr, "usrp_basic_rx: usrp_check_rx_overrun failed\n");
      usb_strerror ();
    }
  }
    
  return r;
}

bool
usrp_basic_rx::set_rx_enable (bool on)
{
  d_rx_enable = on;
  return usrp_set_fpga_rx_enable (d_udh, on);
}

// conditional disable, return prev state
bool
usrp_basic_rx::disable_rx ()
{
  bool enabled = rx_enable ();
  if (enabled)
    set_rx_enable (false);
  return enabled;
}

// conditional set
void
usrp_basic_rx::restore_rx (bool on)
{
  if (on != rx_enable ())
    set_rx_enable (on);
}

static int
slot_id_to_oe_reg (int slot_id)
{
  static int reg[4]  = { FR_OE_0, FR_OE_1, FR_OE_2, FR_OE_3 };
  assert (0 <= slot_id && slot_id < 4);
  return reg[slot_id];
}

static int
slot_id_to_io_reg (int slot_id)
{
  static int reg[4]  = { FR_IO_0, FR_IO_1, FR_IO_2, FR_IO_3 };
  assert (0 <= slot_id && slot_id < 4);
  return reg[slot_id];
}

bool
usrp_basic_rx::_write_oe (int which_dboard, int value, int mask)
{
  if (! (0 <= which_dboard && which_dboard <= 1))
    return false;

  return _write_fpga_reg (slot_id_to_oe_reg (dboard_to_slot (which_dboard)),
			  (mask << 16) | (value & 0xffff));
}

bool
usrp_basic_rx::write_io (int which_dboard, int value, int mask)
{
  if (! (0 <= which_dboard && which_dboard <= 1))
    return false;

  return _write_fpga_reg (slot_id_to_io_reg (dboard_to_slot (which_dboard)),
			  (mask << 16) | (value & 0xffff));
}

bool
usrp_basic_rx::read_io (int which_dboard, int *value)
{
  if (! (0 <= which_dboard && which_dboard <= 1))
    return false;

  int t;
  int reg = which_dboard + 1;	// FIXME, *very* magic number (fix in serial_io.v)
  bool ok = _read_fpga_reg (reg, &t);
  if (!ok)
    return false;

  *value = (t >> 16) & 0xffff;	// FIXME, more magic
  return true;
}

int
usrp_basic_rx::read_io (int which_dboard)
{
  int	value;
  if (!read_io (which_dboard, &value))
    return READ_FAILED;
  return value;
}

bool
usrp_basic_rx::write_aux_dac (int which_dboard, int which_dac, int value)
{
  return usrp_basic::write_aux_dac (dboard_to_slot (which_dboard),
				    which_dac, value);
}

bool
usrp_basic_rx::read_aux_adc (int which_dboard, int which_adc, int *value)
{
  return usrp_basic::read_aux_adc (dboard_to_slot (which_dboard),
				   which_adc, value);
}

int
usrp_basic_rx::read_aux_adc (int which_dboard, int which_adc)
{
  return usrp_basic::read_aux_adc (dboard_to_slot (which_dboard), which_adc);
}

int
usrp_basic_rx::block_size () const { return d_ephandle->block_size(); }

////////////////////////////////////////////////////////////////
//
//			   usrp_basic_tx
//
////////////////////////////////////////////////////////////////


//
// DAC input rate 64 MHz interleaved for a total input rate of 128 MHz
// DAC input is latched on rising edge of CLKOUT2
// NCO is disabled
// interpolate 2x
// coarse modulator disabled
//

usrp_basic_tx::usrp_basic_tx (int which_board)
  : usrp_basic (which_board, open_tx_interface),
    d_devhandle (0), d_ephandle (0),
    d_bytes_seen (0), d_first_write (true),
    d_tx_enable (false)
{

  // Reset the tx path and leave it disabled.
  set_tx_enable (false);
  usrp_set_fpga_tx_reset (d_udh, true);
  usrp_set_fpga_tx_reset (d_udh, false);

  set_fpga_tx_sample_rate_divisor (4);	// we're using interp x4

  // probe_tx_slots (true);

  d_devhandle = fusb_sysconfig::make_devhandle (d_udh);
  d_ephandle = d_devhandle->make_ephandle (USRP_TX_ENDPOINT, false,
					   FUSB_BLOCK_SIZE, FUSB_NBLOCKS);
}


usrp_basic_tx::~usrp_basic_tx ()
{
  d_ephandle->stop ();
  delete d_ephandle;
  delete d_devhandle;

}

bool
usrp_basic_tx::initialize ()
{
  if (!usrp_basic::initialize ())
    return false;
  return true;
}

bool
usrp_basic_tx::start ()
{
  if (!usrp_basic::start ())
    return false;

  if (!set_tx_enable (true)){
    fprintf (stderr, "usrp_basic_tx: set_tx_enable failed\n");
    usb_strerror ();
    return false;
  }
  
  if (!d_ephandle->start ()){
    fprintf (stderr, "usrp_basic_tx: failed to start end point streaming");
    usb_strerror ();
    return false;
  }

  return true;
}

bool
usrp_basic_tx::stop ()
{
  bool ok = usrp_basic::stop ();

  if (!set_tx_enable (false)){
    fprintf (stderr, "usrp_basic_tx: set_tx_enable(false) failed\n");
    usb_strerror ();
    ok = false;
  }
  if (!d_ephandle->stop ()){
    fprintf (stderr, "usrp_basic_tx: failed to stop end point streaming");
    usb_strerror ();
    ok = false;
  }
  return ok;
}

usrp_basic_tx *
usrp_basic_tx::make (int which_board)
{
  usrp_basic_tx *u = 0;
  
  try {
    u = new usrp_basic_tx (which_board);
    if (!u->initialize ()){
      fprintf (stderr, "usrp_basic_tx::make failed to initialize\n");
      throw std::runtime_error ("usrp_basic_tx::make");
    }
    return u;
  }
  catch (...){
    delete u;
    return 0;
  }

  return u;
}

bool
usrp_basic_tx::set_fpga_tx_sample_rate_divisor (unsigned int div)
{
  return _write_fpga_reg (FR_TX_SAMPLE_RATE_DIV, div - 1);
}

/*!
 * \brief Write data to the A/D's via the FPGA.
 *
 * \p len must be a multiple of 512 bytes.
 * \returns number of bytes written or -1 on error.
 *
 * if \p underrun is non-NULL, it will be set to true iff
 * a transmit underrun condition is detected.
 */
int
usrp_basic_tx::write (const void *buf, int len, bool *underrun)
{
  int	r;
  
  if (underrun)
    *underrun = false;
  
  if (len < 0 || (len % 512) != 0){
    fprintf (stderr, "usrp_basic_tx::write: invalid length = %d\n", len);
    return -1;
  }

  r = d_ephandle->write (buf, len);
  if (r > 0)
    d_bytes_seen += r;
    
  /*
   * In many cases, the FPGA reports an tx underrun right after we
   * enable the Tx path.  If this is our first write, check for the
   * underrun to clear the condition, then ignore the result.
   */
  if (d_first_write && d_bytes_seen >= 4 * FUSB_BLOCK_SIZE){
    d_first_write = false;
    bool bogus_underrun;
    usrp_check_tx_underrun (d_udh, &bogus_underrun);
  }

  if (underrun != 0 && d_bytes_seen >= d_bytes_per_poll){
    d_bytes_seen = 0;
    if (!usrp_check_tx_underrun (d_udh, underrun)){
      fprintf (stderr, "usrp_basic_tx: usrp_check_tx_underrun failed\n");
      usb_strerror ();
    }
  }

  return r;
}

void
usrp_basic_tx::wait_for_completion ()
{
  d_ephandle->wait_for_completion ();
}

bool
usrp_basic_tx::set_tx_enable (bool on)
{
  d_tx_enable = on;
  // fprintf (stderr, "set_tx_enable %d\n", on);
  return usrp_set_fpga_tx_enable (d_udh, on);
}

// conditional disable, return prev state
bool
usrp_basic_tx::disable_tx ()
{
  bool enabled = tx_enable ();
  if (enabled)
    set_tx_enable (false);
  return enabled;
}

// conditional set
void
usrp_basic_tx::restore_tx (bool on)
{
  if (on != tx_enable ())
    set_tx_enable (on);
}

bool
usrp_basic_tx::_write_oe (int which_dboard, int value, int mask)
{
  if (! (0 <= which_dboard && which_dboard <= 1))
    return false;

  return _write_fpga_reg (slot_id_to_oe_reg (dboard_to_slot (which_dboard)),
			  (mask << 16) | (value & 0xffff));
}

bool
usrp_basic_tx::write_io (int which_dboard, int value, int mask)
{
  if (! (0 <= which_dboard && which_dboard <= 1))
    return false;

  return _write_fpga_reg (slot_id_to_io_reg (dboard_to_slot (which_dboard)),
			  (mask << 16) | (value & 0xffff));
}

bool
usrp_basic_tx::read_io (int which_dboard, int *value)
{
  if (! (0 <= which_dboard && which_dboard <= 1))
    return false;

  int t;
  int reg = which_dboard + 1;	// FIXME, *very* magic number (fix in serial_io.v)
  bool ok = _read_fpga_reg (reg, &t);
  if (!ok)
    return false;

  *value = t & 0xffff;		// FIXME, more magic
  return true;
}

int
usrp_basic_tx::read_io (int which_dboard)
{
  int	value;
  if (!read_io (which_dboard, &value))
    return READ_FAILED;
  return value;
}

bool
usrp_basic_tx::write_aux_dac (int which_dboard, int which_dac, int value)
{
  return usrp_basic::write_aux_dac (dboard_to_slot (which_dboard),
				    which_dac, value);
}

bool
usrp_basic_tx::read_aux_adc (int which_dboard, int which_adc, int *value)
{
  return usrp_basic::read_aux_adc (dboard_to_slot (which_dboard),
				   which_adc, value);
}

int
usrp_basic_tx::read_aux_adc (int which_dboard, int which_adc)
{
  return usrp_basic::read_aux_adc (dboard_to_slot (which_dboard), which_adc);
}

