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

/*
 * Low level primitives for directly messing with USRP hardware.
 *
 * If you're trying to use the USRP, you'll probably want to take a look
 * at the usrp_rx and usrp_tx classes.  They hide a bunch of low level details
 * and provide high performance streaming i/o.
 *
 * This interface is built on top of libusb, which allegedly works under
 * Linux, *BSD and Mac OS/X.  http://libusb.sourceforge.net
 */

#ifndef _USRP_PRIMS_H_
#define _USRP_PRIMS_H_

#include <usrp_slots.h>
#include <string>

static const int USRP_HASH_SIZE = 16;

enum usrp_load_status_t { ULS_ERROR = 0, ULS_OK, ULS_ALREADY_LOADED };

struct usb_dev_handle;
struct usb_device;

/*!
 * \brief initialize libusb; probe busses and devices.
 * Safe to call more than once.
 */
void usrp_one_time_init ();

/*
 * force a rescan of the buses and devices
 */
void usrp_rescan ();

/*!
 * \brief locate Nth (zero based) USRP device in system.
 * Return pointer or 0 if not found.
 *
 * The following kinds of devices are considered USRPs:
 *
 *   unconfigured USRP (no firwmare loaded)
 *   configured USRP (firmware loaded)
 *   unconfigured Cypress FX2 (only if fx2_ok_p is true)
 */
struct usb_device *usrp_find_device (int nth, bool fx2_ok_p = false);

bool usrp_usrp_p (struct usb_device *q);		//< is this a USRP
bool usrp_usrp0_p (struct usb_device *q);		//< is this a USRP Rev 0
bool usrp_usrp1_p (struct usb_device *q);		//< is this a USRP Rev 1
bool usrp_usrp2_p (struct usb_device *q);		//< is this a USRP Rev 2
bool usrp_fx2_p (struct usb_device *q);			//< is this an unconfigured Cypress FX2

bool usrp_unconfigured_usrp_p (struct usb_device *q);	//< some kind of unconfigured USRP
bool usrp_configured_usrp_p (struct usb_device *q);	//< some kind of configured USRP

/*!
 * \brief given a usb_device return an instance of the appropriate usb_dev_handle
 *
 * These routines claim the specified interface and select the
 * correct alternate interface.  (USB nomenclature is totally screwed!)
 *
 * If interface can't be opened, or is already claimed by some other
 * process, 0 is returned.
 */
struct usb_dev_handle *usrp_open_cmd_interface (struct usb_device *dev);
struct usb_dev_handle *usrp_open_rx_interface (struct usb_device *dev);
struct usb_dev_handle *usrp_open_tx_interface (struct usb_device *dev);

/*!
 * \brief close interface.
 */
bool usrp_close_interface (struct usb_dev_handle *udh);

/*!
 * \brief load intel hex format file into USRP/Cypress FX2 (8051).
 *
 * The filename extension is typically *.ihx
 *
 * Note that loading firmware may cause the device to renumerate.  I.e.,
 * change its configuration, invalidating the current device handle.
 */

usrp_load_status_t 
usrp_load_firmware (struct usb_dev_handle *udh, const char *filename, bool force);

/*!
 * \brief load intel hex format file into USRP FX2 (8051).
 *
 * The filename extension is typically *.ihx
 *
 * Note that loading firmware may cause the device to renumerate.  I.e.,
 * change its configuration, invalidating the current device handle.
 * If the result is ULS_OK, usrp_load_firmware_nth delays 1 second
 * then rescans the busses and devices.
 */
usrp_load_status_t
usrp_load_firmware_nth (int nth, const char *filename, bool force);

/*!
 * \brief load fpga configuration bitstream
 */
usrp_load_status_t
usrp_load_fpga (struct usb_dev_handle *udh, const char *filename, bool force);


bool usrp_set_jtag_opts (struct usb_dev_handle *udh, unsigned char *opts);

/*!
 * \brief load the regular firmware and fpga bitstream in the Nth USRP.
 *
 * This is the normal starting point...
 */
bool usrp_load_standard_bits (int nth, bool force);

/*!
 * \brief copy the given \p hash into the USRP hash slot \p which.
 * The usrp implements two hash slots, 0 and 1.
 */
bool usrp_set_hash (struct usb_dev_handle *udh, int which,
		    const unsigned char hash[USRP_HASH_SIZE]);

/*!
 * \brief retrieve the \p hash from the USRP hash slot \p which.
 * The usrp implements two hash slots, 0 and 1.
 */
bool usrp_get_hash (struct usb_dev_handle *udh, int which,
		    unsigned char hash[USRP_HASH_SIZE]);

bool usrp_write_fpga_reg (struct usb_dev_handle *udh, int reg, int value);
bool usrp_read_fpga_reg (struct usb_dev_handle *udh, int reg, int *value);
bool usrp_set_fpga_reset (struct usb_dev_handle *udh, bool on);
bool usrp_set_fpga_tx_enable (struct usb_dev_handle *udh, bool on);
bool usrp_set_fpga_rx_enable (struct usb_dev_handle *udh, bool on);
bool usrp_set_fpga_tx_reset (struct usb_dev_handle *udh, bool on);
bool usrp_set_fpga_rx_reset (struct usb_dev_handle *udh, bool on);
bool usrp_set_led (struct usb_dev_handle *udh, int which, bool on);

bool usrp_check_rx_overrun (struct usb_dev_handle *udh, bool *overrun_p);
bool usrp_check_tx_underrun (struct usb_dev_handle *udh, bool *underrun_p);

// i2c_read and i2c_write are limited to a maximum len of 64 bytes.

bool usrp_i2c_write (struct usb_dev_handle *udh, int i2c_addr,
		     const void *buf, int len);

bool usrp_i2c_read (struct usb_dev_handle *udh, int i2c_addr,
		    void *buf, int len);

// spi_read and spi_write are limited to a maximum of 64 bytes
// See spi_defs.h for more info

bool usrp_spi_write (struct usb_dev_handle *udh,
		     int optional_header, int enables, int format,
		     unsigned char *buf, int len);

bool usrp_spi_read (struct usb_dev_handle *udh,
		     int optional_header, int enables, int format,
		     unsigned char *buf, int len);


// Slot specific i/o routines

/*!
 * \brief write to the specified aux dac.
 *
 * \p slot: which Tx or Rx slot to write.
 *    N.B., SLOT_TX_A and SLOT_RX_A share the same AUX DAC's
 *          SLOT_TX_B and SLOT_RX_B share the same AUX DAC's
 *
 * \p which_dac: [0,3]  RX slots must use only 0 and 1.
 *			TX slots must use only 2 and 3.
 *
 * AUX DAC 3 is really the 9862 sigma delta output.
 *
 * \p value to write to aux dac.  All dacs take straight
 * binary values.  Although dacs 0, 1 and 2 are 8-bit and dac 3 is 12-bit,
 * the interface is in terms of 12-bit values [0,4095]
 */
bool usrp_write_aux_dac (struct usb_dev_handle *uhd, int slot,
			 int which_dac, int value);

/*!
 * \brief Read the specified aux adc
 *
 * \p slot: which Tx or Rx slot to read aux dac
 * \p which_adc: [0,1]  which of the two adcs to read
 * \p *value: return value, 12-bit straight binary.
 */
bool usrp_read_aux_adc (struct usb_dev_handle *udh, int slot,
			int which_adc, int *value);


/*!
 * \brief usrp daughterboard id to name mapping
 *
 * Returns 0 length string if dbid not known.
 */
const std::string usrp_dbid_to_string (unsigned short dbid);

#endif /* _USRP_PRIMS_H_ */
