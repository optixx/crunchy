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

#include "usrp_prims.h"
#include "usrp_commands.h"
#include "usrp_ids.h"
#include "usrp_i2c_addr.h"
#include <usb.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>		// FIXME should check with autoconf (nanosleep)
#include <algorithm>
#include <assert.h>

#define VERBOSE 0

static const int FIRMWARE_HASH_SLOT	= 0;
static const int FPGA_HASH_SLOT 	= 1;

static const int hash_slot_addr[2] = {
  USRP_HASH_SLOT_0_ADDR,
  USRP_HASH_SLOT_1_ADDR
};

static const char *usrp_firmware_filename = "usrp_firmware.ihx";
static const char *usrp_fpga_filename = "usrp_fpga.rbf";

#include "std_paths.h"

static char *
find_file (const char *filename, int hw_rev)
{
  const char **sp = std_paths;
  static char path[1000];

  while (*sp){
    snprintf (path, sizeof (path), "%s/rev%d/%s", *sp, hw_rev, filename);
    if (access (path, R_OK) == 0)
      return path;
    sp++;
  }
  return 0;
}

void
usrp_one_time_init ()
{
  static bool first = true;

  if (first){
    first = false;
    usb_init ();			// usb library init
    usb_find_busses ();
    usb_find_devices ();
  }
}

void
usrp_rescan ()
{
  usb_find_busses ();
  usb_find_devices ();
}


// ----------------------------------------------------------------
// Danger, big, fragile KLUDGE.  The problem is that we want to be
// able to get from a usb_dev_handle back to a usb_device, and the
// right way to do this is buried in a non-installed include file.

static struct usb_device *
dev_handle_to_dev (usb_dev_handle *udh)
{
  struct usb_dev_handle_kludge {
    int			 fd;
    struct usb_bus	*bus;
    struct usb_device	*device;
  };

  return ((struct usb_dev_handle_kludge *) udh)->device;
}

// ----------------------------------------------------------------

/*
 * q must be a real USRP, not an FX2.  Return its hardware rev number.
 */
static int
_usrp_hw_rev (struct usb_device *q)
{
  return q->descriptor.bcdDevice & 0x00FF;
}

/*
 * q must be a real USRP, not an FX2.  Return true if it's configured.
 */
static bool
_usrp_configured_p (struct usb_device *q)
{
  return (q->descriptor.bcdDevice & 0xFF00) != 0;
}

bool
usrp_usrp_p (struct usb_device *q)
{
  return (q->descriptor.idVendor == USB_VID_FSF
	  && ((q->descriptor.idProduct == USB_PID_FSF_USRP) ||
	      (q->descriptor.idProduct == USB_PID_FSF_LBNL_UXO) ||
	      (q->descriptor.idProduct == USB_PID_FSF_LBNL_LLRF4) ||
	      (q->descriptor.idProduct == USB_PID_FSF_LBNL_LLRF41)) );
}

bool
usrp_fx2_p (struct usb_device *q)
{
  return (q->descriptor.idVendor == USB_VID_CYPRESS
	  && q->descriptor.idProduct == USB_PID_CYPRESS_FX2);
}

bool
usrp_usrp0_p (struct usb_device *q)
{
  return usrp_usrp_p (q) && _usrp_hw_rev (q) == 0;
}

bool
usrp_usrp1_p (struct usb_device *q)
{
  return usrp_usrp_p (q) && _usrp_hw_rev (q) == 1;
}

bool
usrp_usrp2_p (struct usb_device *q)
{
  return usrp_usrp_p (q) && _usrp_hw_rev (q) == 2;
}


bool
usrp_unconfigured_usrp_p (struct usb_device *q)
{
  return usrp_usrp_p (q) && !_usrp_configured_p (q);
}

bool
usrp_configured_usrp_p (struct usb_device *q)
{
  return usrp_usrp_p (q) && _usrp_configured_p (q);
}

// ----------------------------------------------------------------

struct usb_device *
usrp_find_device (int nth, bool fx2_ok_p)
{
  struct usb_bus *p;
  struct usb_device *q;
  int	 n_found = 0;

  usrp_one_time_init ();
  
  p = usb_get_busses();
  while (p != NULL){
    q = p->devices;
    while (q != NULL){
      if (usrp_usrp_p (q) || (fx2_ok_p && usrp_fx2_p (q))){
	if (n_found == nth)	// return this one
	  return q;
	n_found++;		// keep looking
      }
      q = q->next;
    }
    p = p->next;
  }
  return 0;	// not found
}

static struct usb_dev_handle *
usrp_open_interface (struct usb_device *dev, int interface, int altinterface)
{
  struct usb_dev_handle *udh = usb_open (dev);
  if (udh == 0)
    return 0;

  if (dev != dev_handle_to_dev (udh)){
    fprintf (stderr, "%s:%d: internal error!\n", __FILE__, __LINE__);
    abort ();
  }

  if (usb_set_configuration (udh, 1) < 0){
    fprintf (stderr, "%s: usb_claim_interface: failed conf %d\n", __FUNCTION__,interface);
    fprintf (stderr, "%s\n", usb_strerror());
    usb_close (udh);
    return 0;
  }

  if (usb_claim_interface (udh, interface) < 0){
    fprintf (stderr, "%s:usb_claim_interface: failed interface %d\n", __FUNCTION__,interface);
    fprintf (stderr, "%s\n", usb_strerror());
    usb_close (udh);
    return 0;
  }

  if (usb_set_altinterface (udh, altinterface) < 0){
    fprintf (stderr, "%s:usb_set_alt_interface: failed\n", __FUNCTION__);
    fprintf (stderr, "%s\n", usb_strerror());
    usb_release_interface (udh, interface);
    usb_close (udh);
    return 0;
  }

  return udh;
}

struct usb_dev_handle *
usrp_open_cmd_interface (struct usb_device *dev)
{
  return usrp_open_interface (dev, USRP_CMD_INTERFACE, USRP_CMD_ALTINTERFACE);
}

struct usb_dev_handle *
usrp_open_rx_interface (struct usb_device *dev)
{
  return usrp_open_interface (dev, USRP_RX_INTERFACE, USRP_RX_ALTINTERFACE);
}

struct usb_dev_handle *
usrp_open_tx_interface (struct usb_device *dev)
{
  return usrp_open_interface (dev, USRP_TX_INTERFACE, USRP_TX_ALTINTERFACE);
}

bool
usrp_close_interface (struct usb_dev_handle *udh)
{
  // we're assuming that closing an interface automatically releases it.
  return usb_close (udh) == 0;
}

// ----------------------------------------------------------------
// write internal ram using Cypress vendor extension

static bool
write_internal_ram (struct usb_dev_handle *udh, unsigned char *buf,
		    int start_addr, size_t len)
{
  int addr;
  int n;
  int a;
  int quanta = MAX_EP0_PKTSIZE;

  for (addr = start_addr; addr < start_addr + (int) len; addr += quanta){
    n = len + start_addr - addr;
    if (n > quanta)
      n = quanta;

    a = usb_control_msg (udh, 0x40, 0xA0,
			 addr, 0, (char *)(buf + (addr - start_addr)), n, 1000);

    if (a < 0){
      fprintf(stderr,"write_internal_ram failed: %s\n", usb_strerror());
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------
// whack the CPUCS register using the upload RAM vendor extension

static bool
reset_cpu (struct usb_dev_handle *udh, bool reset_p)
{
  unsigned char v;

  if (reset_p)
    v = 1;		// hold processor in reset
  else
    v = 0;	        // release reset

  return write_internal_ram (udh, &v, 0xE600, 1);
}

// ----------------------------------------------------------------
// Load intel format file into cypress FX2 (8051)

static bool
_usrp_load_firmware (struct usb_dev_handle *udh, const char *filename,
		     unsigned char hash[USRP_HASH_SIZE])
{
  FILE	*f = fopen (filename, "ra");
  if (f == 0){
    perror (filename);
    return false;
  }

  if (!reset_cpu (udh, true))	// hold CPU in reset while loading firmware
    goto fail;

  
  char s[1024];
  int length;
  int addr;
  int type;
  unsigned char data[256];
  unsigned char checksum, a;
  unsigned int b;
  int i;

  while (!feof(f)){
    fgets(s, sizeof (s), f); /* we should not use more than 263 bytes normally */
    if(s[0]!=':'){
      fprintf(stderr,"%s: invalid line: \"%s\"\n", filename, s);
      goto fail;
    }
    sscanf(s+1, "%02x", &length);
    sscanf(s+3, "%04x", &addr);
    sscanf(s+7, "%02x", &type);

    if(type==0){

      a=length+(addr &0xff)+(addr>>8)+type;
      for(i=0;i<length;i++){
	sscanf (s+9+i*2,"%02x", &b);
	data[i]=b;
	a=a+data[i];
      }

      sscanf (s+9+length*2,"%02x", &b);
      checksum=b;
      if (((a+checksum)&0xff)!=0x00){
	fprintf (stderr, "  ** Checksum failed: got 0x%02x versus 0x%02x\n", (-a)&0xff, checksum);
	goto fail;
      }
      if (!write_internal_ram (udh, data, addr, length))
	goto fail;
    }
    else if (type == 0x01){      // EOF
      break;
    }
    else if (type == 0x02){
      fprintf(stderr, "Extended address: whatever I do with it?\n");
      fprintf (stderr, "%s: invalid line: \"%s\"\n", filename, s);
      goto fail;
    }
  }

  // we jam the hash value into the FX2 memory before letting
  // the cpu out of reset.  When it comes out of reset it
  // may renumerate which will invalidate udh.

  if (!usrp_set_hash (udh, FIRMWARE_HASH_SLOT, hash))
    fprintf (stderr, "usrp: failed to write firmware hash slot\n");

  if (!reset_cpu (udh, false))		// take CPU out of reset
    goto fail;

  fclose (f);
  return true;

 fail:
  fclose (f);
  return false;
}

// ----------------------------------------------------------------
// write vendor extension command to USRP

static int
write_cmd (struct usb_dev_handle *udh,
	   int request, int value, int index,
	   unsigned char *bytes, int len)
{
  int	requesttype = (request & 0x80) ? VRT_VENDOR_IN : VRT_VENDOR_OUT;

  int r = usb_control_msg (udh, requesttype, request, value, index,
			   (char *) bytes, len, 1000);
  if (r < 0){
    // we get EPIPE if the firmware stalls the endpoint.
    if (errno != EPIPE)
      fprintf (stderr, "usb_control_msg failed: %s\n", usb_strerror ());
  }

  return r;
}

bool usrp_set_jtag_opts(struct usb_dev_handle *udh, unsigned char *opts)
{
	int r = usb_control_msg (udh, 0x40, 0xa0, USRP_JTAG_CONFIG_ADDR, 0,
			(char *) opts, 16, 1000);
	return r==16;
}

// ----------------------------------------------------------------
// load fpga

static bool
_usrp_load_fpga (struct usb_dev_handle *udh, const char *filename,
		 unsigned char hash[USRP_HASH_SIZE])
{
  unsigned char s3_init[]={
	0x00, 0x00, 0x00, 0x00, // Flush
        0x00, 0x00, 0x00, 0x00, // Flush
        0xe0, 0x00, 0x00, 0x00, // Clear CRC
        0x80, 0x01, 0x00, 0x0c, // CMD
        0x66, 0xAA, 0x99, 0x55, // Sync
        0xff, 0xff, 0xff, 0xff  // Sync
  };
  unsigned char s3_hdr[]={
	0x00, 0x00, 0x00, 0x00, // Flush
#if 0
        0x10, 0x00, 0x00, 0x00, // Assert GHIGH
        0x80, 0x01, 0x00, 0x0c  // CMD
#else
	0x00, 0x00, 0x00, 0x00, // Flush
	0x00, 0x00, 0x00, 0x00  // Flush
#endif
  };
  bool ok = true;

  FILE	*fp = fopen (filename, "rb");
  if (fp == 0){
    perror (filename);
    return false;
  }

  unsigned char buf[MAX_EP0_PKTSIZE];	// 64 is max size of EP0 packet on FX2
  int n;

  fputs("setting led\n", stderr);
  usrp_set_led (udh, 1, 1);		// led 1 on


  struct usb_device *q = dev_handle_to_dev(udh);
  if (q->descriptor.idProduct != USB_PID_FSF_LBNL_LLRF41) {
    fputs("reset fpga\n", stderr);
    usrp_set_fpga_reset (udh, 1);		// hold fpga in reset
  } else {
    fputs("alternate reset fpga\n", stderr);
    if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_BEGIN, 0, 0) != 0) goto fail;
    if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_XFER, s3_init, 24) != 24) goto fail;
    if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_END_SHUTDOWN, 0, 0) != 0) goto fail;
  }

  fputs("fpga_load begin\n", stderr);
  if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_BEGIN, 0, 0) != 0)
    goto fail;

  if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_XFER, s3_hdr, 12) != 12) goto fail;
  while ((n = fread (buf, 1, sizeof (buf), fp)) > 0){
    if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_XFER, buf, n) != n)
      goto fail;
  }

  fputs("fpga_load end\n", stderr);
  if (write_cmd (udh, VRQ_FPGA_LOAD, 0, FL_END, 0, 0) != 0)
    goto fail;
  
  fclose (fp);

  if (!usrp_set_hash (udh, FPGA_HASH_SLOT, hash))
    fprintf (stderr, "usrp: failed to write fpga hash slot\n");

  // On the rev1 USRP, the {tx,rx}_{enable,reset} bits are
  // controlled over the serial bus, and hence aren't observed until
  // we've got a good fpga bitstream loaded.

  usrp_set_fpga_reset (udh, 0);		// fpga out of master reset

  // now these commands will work
  
  ok &= usrp_set_fpga_tx_enable (udh, 0);
  ok &= usrp_set_fpga_rx_enable (udh, 0);

  if (_usrp_hw_rev (dev_handle_to_dev (udh)) != 0){
    ok &= usrp_set_fpga_tx_reset (udh, 1);	// reset tx and rx  paths
    ok &= usrp_set_fpga_rx_reset (udh, 1);
    ok &= usrp_set_fpga_tx_reset (udh, 0);	// reset tx and rx  paths
    ok &= usrp_set_fpga_rx_reset (udh, 0);
  }

  if (!ok)
    fprintf (stderr, "usrp: failed to reset tx and/or rx path\n");

  usrp_set_led (udh, 1, 0);		// led 1 off

  return true;

 fail:
  fclose (fp);
  return false;
}

// ----------------------------------------------------------------

bool 
usrp_set_led (struct usb_dev_handle *udh, int which, bool on)
{
  int r = write_cmd (udh, VRQ_SET_LED, on, which, 0, 0);

  return r == 0;
}

bool
usrp_set_hash (struct usb_dev_handle *udh, int which,
	       const unsigned char hash[USRP_HASH_SIZE])
{
  which &= 1;
  
  // we use the Cypress firmware down load command to jam it in.
  int r = usb_control_msg (udh, 0x40, 0xa0, hash_slot_addr[which], 0,
			   (char *) hash, USRP_HASH_SIZE, 1000);
  return r == USRP_HASH_SIZE;
}

bool
usrp_get_hash (struct usb_dev_handle *udh, int which, 
	       unsigned char hash[USRP_HASH_SIZE])
{
  which &= 1;
  
  // we use the Cypress firmware upload command to fetch it.
  int r = usb_control_msg (udh, 0xc0, 0xa0, hash_slot_addr[which], 0,
			   (char *) hash, USRP_HASH_SIZE, 1000);
  return r == USRP_HASH_SIZE;
}

static bool
usrp_set_switch (struct usb_dev_handle *udh, int cmd_byte, bool on)
{
  return write_cmd (udh, cmd_byte, on, 0, 0, 0) == 0;
}


static bool
usrp0_fpga_write (struct usb_dev_handle *udh, int reg, int value)
{
  // on the rev0 usrp, we use thd FPGA_WRITE_REG command
  
  unsigned char buf[4];

  buf[0] = (value >> 24) & 0xff;
  buf[1] = (value >> 16) & 0xff;
  buf[2] = (value >>  8) & 0xff;
  buf[3] = (value >>  0) & 0xff;

  return write_cmd (udh, VRQ_FPGA_WRITE_REG,
		    0, reg & 0xff, buf, sizeof (buf)) == sizeof (buf);
}

static bool
usrp1_fpga_write (struct usb_dev_handle *udh,
		  int regno, int value)
{
  // on the rev1 usrp, we use the generic spi_write interface

  unsigned char buf[4];

  buf[0] = (value >> 24) & 0xff;	// MSB first
  buf[1] = (value >> 16) & 0xff;
  buf[2] = (value >>  8) & 0xff;
  buf[3] = (value >>  0) & 0xff;
  
  return usrp_spi_write (udh, 0x00 | (regno & 0x7f),
			 SPI_ENABLE_FPGA,
			 SPI_FMT_MSB | SPI_FMT_HDR_1,
			 buf, sizeof (buf));
}

static bool
usrp1_fpga_read (struct usb_dev_handle *udh,
		 int regno, int *value)
{
  *value = 0;
  unsigned char buf[4];

  bool ok = usrp_spi_read (udh, 0x80 | (regno & 0x7f),
			   SPI_ENABLE_FPGA,
			   SPI_FMT_MSB | SPI_FMT_HDR_1,
			   buf, sizeof (buf));

  if (ok)
    *value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

  return ok;
}


bool
usrp_write_fpga_reg (struct usb_dev_handle *udh, int reg, int value)
{
  switch (_usrp_hw_rev (dev_handle_to_dev (udh))){
  case 0:
    return usrp0_fpga_write (udh, reg, value);
  case 1:
  case 2:
    return usrp1_fpga_write (udh, reg, value);
  default:
    abort ();
  }
}

bool
usrp_read_fpga_reg (struct usb_dev_handle *udh, int reg, int *value)
{
  switch (_usrp_hw_rev (dev_handle_to_dev (udh))){
  case 0:
    return false;		// no read back
  case 1:
  case 2:
    return usrp1_fpga_read (udh, reg, value);
  default:
    abort ();
  }
}

bool 
usrp_set_fpga_reset (struct usb_dev_handle *udh, bool on)
{
  return usrp_set_switch (udh, VRQ_FPGA_SET_RESET, on);
}

bool 
usrp_set_fpga_tx_enable (struct usb_dev_handle *udh, bool on)
{
  return usrp_set_switch (udh, VRQ_FPGA_SET_TX_ENABLE, on);
}

bool 
usrp_set_fpga_rx_enable (struct usb_dev_handle *udh, bool on)
{
  return usrp_set_switch (udh, VRQ_FPGA_SET_RX_ENABLE, on);
}

bool 
usrp_set_fpga_tx_reset (struct usb_dev_handle *udh, bool on)
{
  return usrp_set_switch (udh, VRQ_FPGA_SET_TX_RESET, on);
}

bool 
usrp_set_fpga_rx_reset (struct usb_dev_handle *udh, bool on)
{
  return usrp_set_switch (udh, VRQ_FPGA_SET_RX_RESET, on);
}

static usrp_load_status_t
usrp_conditionally_load_something (struct usb_dev_handle *udh,
				   const char *filename,
				   bool force,
				   int slot,
				   bool loader (struct usb_dev_handle *,
						const char *,
						unsigned char [USRP_HASH_SIZE]))
{
  unsigned char file_hash[USRP_HASH_SIZE];
  
  if (access (filename, R_OK) != 0){
    perror (filename);
    return ULS_ERROR;
  }

  // XXX deleted conditionals
 
  bool r = loader (udh, filename, file_hash);

  if (!r)
    return ULS_ERROR;

  return ULS_OK;
}

usrp_load_status_t
usrp_load_firmware (struct usb_dev_handle *udh,
		    const char *filename,
		    bool force)
{
  return usrp_conditionally_load_something (udh, filename, force,
					    FIRMWARE_HASH_SLOT,
					    _usrp_load_firmware);
}

usrp_load_status_t
usrp_load_fpga (struct usb_dev_handle *udh,
		const char *filename,
		bool force)
{
  return usrp_conditionally_load_something (udh, filename, force,
					    FPGA_HASH_SLOT,
					    _usrp_load_fpga);
}

static usb_dev_handle *
open_nth_cmd_interface (int nth)
{
  struct usb_device *udev = usrp_find_device (nth);
  if (udev == 0){
    fprintf (stderr, "usrp: failed to find usrp[%d]\n", nth);
    return 0;
  }

  struct usb_dev_handle *udh;

  udh = usrp_open_cmd_interface (udev);
  if (udh == 0){
    // FIXME this could be because somebody else has it open.
    // We should delay and retry...
    fprintf (stderr, "open_nth_cmd_interface: open_cmd_interface failed\n");
    usb_strerror ();
    return 0;
  }

  return udh;
 }

static bool
our_nanosleep (const struct timespec *delay)
{
  struct timespec	new_delay = *delay;
  struct timespec	remainder;

  while (1){
    int r = nanosleep (&new_delay, &remainder);
    if (r == 0)
      return true;
    if (errno == EINTR)
      new_delay = remainder;
    else {
      perror ("nanosleep");
      return false;
    }
  }
}

usrp_load_status_t
usrp_load_firmware_nth (int nth, const char *filename, bool force){
  struct usb_dev_handle *udh = open_nth_cmd_interface (nth);
  if (udh == 0)
    return ULS_ERROR;

  usrp_load_status_t s = usrp_load_firmware (udh, filename, force);
  usrp_close_interface (udh);

  switch (s){

  case ULS_ALREADY_LOADED:		// nothing changed...
    return ULS_ALREADY_LOADED;
    break;

  case ULS_OK:
    // we loaded firmware successfully.

    // It's highly likely that the board will renumerate (simulate a
    // disconnect/reconnect sequence), invalidating our current
    // handle.

    // FIXME.  Turn this into a loop that rescans until we refind ourselves
    
    struct timespec	t;	// delay for 1 second
    t.tv_sec = 2;
    t.tv_nsec = 0;
    our_nanosleep (&t);

    usb_find_busses ();		// rescan busses and devices
    usb_find_devices ();

    return ULS_OK;

  default:
  case ULS_ERROR:		// some kind of problem
    return ULS_ERROR;
  }
}

static void
load_status_msg (usrp_load_status_t s, const char *type, const char *filename)
{
  switch (s){
  case ULS_ERROR:
    fprintf (stderr, "usrp: failed to load %s %s.\n", type, filename);
    break;
    
#if (VERBOSE)
  case ULS_ALREADY_LOADED:
    fprintf (stderr, "usrp: %s %s already loaded.\n", type, filename);
    break;

  case ULS_OK:
    fprintf (stderr, "usrp: %s %s loaded successfully.\n", type, filename);
    break;
#else
  case ULS_ALREADY_LOADED:
  case ULS_OK:
    break;
#endif
  }
}

bool
usrp_load_standard_bits (int nth, bool force)
{
  usrp_load_status_t 	s;
  char			*filename;
  int hw_rev;

  // first, figure out what hardware rev we're dealing with
  {
    struct usb_device *udev = usrp_find_device (nth);
    if (udev == 0){
      fprintf (stderr, "usrp: failed to find usrp[%d]\n", nth);
      return false;
    }
    hw_rev = _usrp_hw_rev (udev);
  }

  // start by loading the firmware

  filename = find_file (usrp_firmware_filename, hw_rev);
  if (filename == 0){
    fprintf (stderr, "Can't find firmware: %s\n", usrp_firmware_filename);
    return false;
  }

  s = usrp_load_firmware_nth (nth, filename, force);
  load_status_msg (s, "firmware", filename);

  if (s == ULS_ERROR)
    return false;

  // if we actually loaded firmware, we must reload fpga ...
  if (s == ULS_OK)
    force = true;

  // now move on to the fpga configuration bitstream

  filename = find_file (usrp_fpga_filename, hw_rev);
  if (filename == 0){
    fprintf (stderr, "Can't find fpga bitstream: %s\n", usrp_fpga_filename);
    return false;
  }

  struct usb_dev_handle *udh = open_nth_cmd_interface (nth);
  if (udh == 0)
    return false;
  
  s = usrp_load_fpga (udh, filename, force);
  usrp_close_interface (udh);
  load_status_msg (s, "fpga bitstream", filename);

  if (s == ULS_ERROR)
    return false;

  return true;
}

bool
_usrp_get_status (struct usb_dev_handle *udh, int which, bool *trouble)
{
  unsigned char	status;
  *trouble = true;
  
  if (write_cmd (udh, VRQ_GET_STATUS, 0, which,
		 &status, sizeof (status)) != sizeof (status))
    return false;

  *trouble = status;
  return true;
}

bool
usrp_check_rx_overrun (struct usb_dev_handle *udh, bool *overrun_p)
{
  return _usrp_get_status (udh, GS_RX_OVERRUN, overrun_p);
}

bool
usrp_check_tx_underrun (struct usb_dev_handle *udh, bool *underrun_p)
{
  return _usrp_get_status (udh, GS_TX_UNDERRUN, underrun_p);
}


bool
usrp_i2c_write (struct usb_dev_handle *udh, int i2c_addr,
		const void *buf, int len)
{
  if (len < 1 || len > MAX_EP0_PKTSIZE)
    return false;

  return write_cmd (udh, VRQ_I2C_WRITE, i2c_addr, 0,
		    (unsigned char *) buf, len) == len;
}


bool
usrp_i2c_read (struct usb_dev_handle *udh, int i2c_addr,
	       void *buf, int len)
{
  if (len < 1 || len > MAX_EP0_PKTSIZE)
    return false;

  return write_cmd (udh, VRQ_I2C_READ, i2c_addr, 0,
		    (unsigned char *) buf, len) == len;
}

bool
usrp_spi_write (struct usb_dev_handle *udh,
		int optional_header, int enables, int format,
		unsigned char *buf, int len)
{
  if (len < 0 || len > MAX_EP0_PKTSIZE)
    return false;

  return write_cmd (udh, VRQ_SPI_WRITE,
		    optional_header,
		    ((enables & 0xff) << 8) | (format & 0xff),
		    buf, len) == len;
}


bool
usrp_spi_read (struct usb_dev_handle *udh,
	       int optional_header, int enables, int format,
	       unsigned char *buf, int len)
{
  if (len < 0 || len > MAX_EP0_PKTSIZE)
    return false;

  return write_cmd (udh, VRQ_SPI_READ,
		    optional_header,
		    ((enables & 0xff) << 8) | (format & 0xff),
		    buf, len) == len;
}

// ----------------------------------------------------------------

static bool
slot_to_codec (int slot, int *which_codec)
{
  *which_codec = 0;
  
  switch (slot){
  case SLOT_TX_A:
  case SLOT_RX_A:
    *which_codec = 0;
    break;

  case SLOT_TX_B:
  case SLOT_RX_B:
    *which_codec = 1;
    break;

  default:
    fprintf (stderr, "usrp_prims:slot_to_codec: invalid slot = %d\n", slot);
    return false;
  }
  return true;
}

#if 0
static bool
tx_slot_p (int slot)
{
  switch (slot){
  case SLOT_TX_A:
  case SLOT_TX_B:
    return true;

  default:
    return false;
  }
}
#endif

bool
usrp_write_aux_dac (struct usb_dev_handle *udh, int slot,
		    int which_dac, int value)
{
  int which_codec;
  
  if (!slot_to_codec (slot, &which_codec))
    return false;

  if (!(0 <= which_dac && which_dac < 4)){
    fprintf (stderr, "usrp_write_aux_dac: invalid dac = %d\n", which_dac);
    return false;
  }
  // XXX deleted
  return false;
}


bool
usrp_read_aux_adc (struct usb_dev_handle *udh, int slot,
		   int which_adc, int *value)
{
  *value = 0;
  int	which_codec;

  if (!slot_to_codec (slot, &which_codec))
    return false;

  if (!(0 <= which_codec && which_codec < 2)){
    fprintf (stderr, "usrp_read_aux_adc: invalid adc = %d\n", which_adc);
    return false;
  }

  // XXX deleted
  return false;
}

