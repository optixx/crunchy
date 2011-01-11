/* -*- c++ -*- */
/*
 * USRP - Universal Software Radio Peripheral
 *
 * Copyright (C) 2003,2004 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>			/* needed for usb functions */
#include <getopt.h>
#include <assert.h>
#include <errno.h>

#include "usrp_prims.h"
#include "usrp_spi_defs.h"

char *prog_name;


static void
set_progname (char *path)
{
  char *p = strrchr (path, '/');
  if (p != 0)
    prog_name = p+1;
  else
    prog_name = path;
}

static unsigned int from_hex(int x) {
	unsigned int u=0;
	if (isdigit(x)) u = x-'0';
	else if (isalpha(x)) u = x-'A'+10;
	u = u & 0xf;
	return u;
}

static void
usage ()
{
  fprintf (stderr, "usage: \n");
  fprintf (stderr, "  %s [-v] [-w <which_board>] [-x] ...\n", prog_name);
  fprintf (stderr, "  %s load_standard_bits\n", prog_name);
  fprintf (stderr, "  %s load_firmware <file.ihx>\n", prog_name);
  fprintf (stderr, "  %s load_fpga <file.rbf>\n", prog_name);
  fprintf (stderr, "  %s write_fpga_reg <reg8> <value32>\n", prog_name);
  fprintf (stderr, "  %s set_fpga_reset {on|off}\n", prog_name);
  fprintf (stderr, "  %s set_fpga_tx_enable {on|off}\n", prog_name);
  fprintf (stderr, "  %s set_fpga_rx_enable {on|off}\n", prog_name);
#ifdef UNHACKED_USRP
  fprintf (stderr, "  %s set_sleep_bits <bits4> <mask4>\n", prog_name);
#endif
  fprintf (stderr, "  ----- diagnostic routines -----\n");
  fprintf (stderr, "  %s led0 {on|off}\n", prog_name);
  fprintf (stderr, "  %s led1 {on|off}\n", prog_name);
  fprintf (stderr, "  %s set_hash0 <hex-string>\n", prog_name);
  fprintf (stderr, "  %s get_hash0\n", prog_name);
  fprintf (stderr, "  %s i2c_read i2c_addr len\n", prog_name);
  fprintf (stderr, "  %s i2c_write i2c_addr <hex-string>\n", prog_name);
#ifdef UNHACKED_USRP
  fprintf (stderr, "  %s 9862a_write regno value\n", prog_name);
  fprintf (stderr, "  %s 9862b_write regno value\n", prog_name);
  fprintf (stderr, "  %s 9862a_read regno\n", prog_name);
  fprintf (stderr, "  %s 9862b_read regno\n", prog_name);
#endif
  exit (1);
}

#ifdef UNHACKED_USRP
static void
die (const char *msg)
{
  fprintf (stderr, "%s (die): %s\n", prog_name, msg);
  exit (1);
}
#endif

static int 
hexval (char ch)
{
  if ('0' <= ch && ch <= '9')
    return ch - '0';

  if ('a' <= ch && ch <= 'f')
    return ch - 'a' + 10;

  if ('A' <= ch && ch <= 'F')
    return ch - 'A' + 10;

  return -1;
}

static unsigned char *
hex_string_to_binary (const char *string, int *lenptr)
{
  int	sl = strlen (string);
  if (sl & 0x01){
    fprintf (stderr, "%s: odd number of chars in <hex-string>\n", prog_name);
    return 0;
  }

  int len = sl / 2;
  *lenptr = len;
  unsigned char *buf = new unsigned char [len];

  for (int i = 0; i < len; i++){
    int hi = hexval (string[2 * i]);
    int lo = hexval (string[2 * i + 1]);
    if (hi < 0 || lo < 0){
      fprintf (stderr, "%s: invalid char in <hex-string>\n", prog_name);
      delete [] buf;
      return 0;
    }
    buf[i] = (hi << 4) | lo;
  }
  return buf;
}

static void
print_hex (FILE *fp, unsigned char *buf, int len)
{
  for (int i = 0; i < len; i++){
    fprintf (fp, "%02x", buf[i]);
  }
  fprintf (fp, "\n");
}

static void
chk_result (bool ok)
{
  if (!ok){
    fprintf (stderr, "%s: failed\n", prog_name);
    exit (1);
  }
}

static bool
get_on_off (const char *s)
{
  if (strcmp (s, "on") == 0)
    return true;

  if (strcmp (s, "off") == 0)
    return false;

  usage ();			// no return
  return false;
}


int
main (int argc, char **argv)
{
  int		ch;
  bool		verbose = false;
  int		which_board = 0;
  bool		fx2_ok_p = false;
  
  set_progname (argv[0]);
  
  while ((ch = getopt (argc, argv, "vw:x")) != EOF){
    switch (ch){

    case 'v':
      verbose = true;
      break;
      
    case 'w':
      which_board = strtol (optarg, 0, 0);
      break;
      
    case 'x':
      fx2_ok_p = true;
      break;
      
    default:
      usage ();
    }
  }

  int nopts = argc - optind;

  if (nopts < 1)
    usage ();

  const char *cmd = argv[optind++];
  nopts--;

  usrp_one_time_init ();

  
  struct usb_device *udev = usrp_find_device (which_board, fx2_ok_p);
  if (udev == 0){
    fprintf (stderr, "%s: failed to find usrp[%d]\n", prog_name, which_board);
    exit (1);
  }

  if (usrp_unconfigured_usrp_p (udev)){
    fprintf (stderr, "%s: found unconfigured usrp; needs firmware.\n", prog_name);
  }

  if (usrp_fx2_p (udev)){
    fprintf (stderr, "%s: found unconfigured FX2; needs firmware.\n", prog_name);
  }

  struct usb_dev_handle *udh = usrp_open_cmd_interface (udev);
  if (udh == 0){
    fprintf (stderr, "%s: failed to open_cmd_interface\n", prog_name);
    exit (1);
  }

#define CHKARGS(n) if (nopts != n) usage (); else

  if (strcmp (cmd, "led0") == 0) { CHKARGS (1) {
    bool on = get_on_off (argv[optind]);
    chk_result (usrp_set_led (udh, 0, on));
  }}
  else if (strcmp (cmd, "led1") == 0) { CHKARGS (1) {
    bool on = get_on_off (argv[optind]);
    chk_result (usrp_set_led (udh, 1, on));
  }}
  else if (strcmp (cmd, "led2") == 0) { CHKARGS (1) {
    bool on = get_on_off (argv[optind]);
    chk_result (usrp_set_led (udh, 2, on));
  }}
  else if (strcmp (cmd, "set_hash0") == 0) { CHKARGS (1) {
    char *p = argv[optind];
    unsigned char buf[16];

    memset (buf, ' ', 16);
    for (int i = 0; i < 16 && *p; i++)
      buf[i] = *p++;
    
    chk_result (usrp_set_hash (udh, 0, buf));
  }}
  else if (strcmp (cmd, "get_hash0") == 0) { CHKARGS (0) {
    unsigned char buf[17];
    memset (buf, 0, 17);
    bool r = usrp_get_hash (udh, 0, buf);
    if (r)
      printf ("hash: %s\n", buf);
    chk_result (r);
  }}
  else if (strcmp (cmd, "load_fpga") == 0) { CHKARGS (1) {
    char *filename = argv[optind];
    chk_result (usrp_load_fpga (udh, filename, true));
  }}
  else if (strcmp (cmd, "load_firmware") == 0) { CHKARGS (1) {
    char *filename = argv[optind];
    chk_result (usrp_load_firmware (udh, filename, true));
  }}
  else if (strcmp (cmd, "write_fpga_reg") == 0) { CHKARGS (2) {
    chk_result (usrp_write_fpga_reg (udh, strtoul (argv[optind], 0, 0),
				     strtoul(argv[optind+1], 0, 0)));
  }}
  else if (strcmp (cmd, "set_fpga_reset") == 0) { CHKARGS (1) {
    chk_result (usrp_set_fpga_reset (udh, get_on_off (argv[optind])));
  }}
  else if (strcmp (cmd, "set_fpga_tx_enable") == 0) { CHKARGS (1) {
    chk_result (usrp_set_fpga_tx_enable (udh, get_on_off (argv[optind])));
  }}
  else if (strcmp (cmd, "set_fpga_rx_enable") == 0) { CHKARGS (1) {
    chk_result (usrp_set_fpga_rx_enable (udh, get_on_off (argv[optind])));
  }}
#ifdef UNHACKED_USRP
  else if (strcmp (cmd, "set_sleep_bits") == 0) { CHKARGS (2) {
    chk_result (usrp_set_sleep_bits (udh,
				     strtol (argv[optind], 0, 0),
				     strtol (argv[optind+1], 0, 0)));
  }}
#endif
  else if (strcmp (cmd, "load_standard_bits") == 0) { CHKARGS (0) {
    usrp_close_interface (udh);
    udh = 0;
    chk_result (usrp_load_standard_bits (which_board, true));
  }}
  else if (strcmp (cmd, "i2c_read") == 0) { CHKARGS (2) {
    int	i2c_addr = strtol (argv[optind], 0, 0);
    int len = strtol (argv[optind + 1], 0, 0);
    if (len < 0)
      chk_result (0);

    unsigned char *buf = new unsigned char [len];
    bool result = usrp_i2c_read (udh, i2c_addr, buf, len);
    if (!result){
      chk_result (0);
    }
    print_hex (stdout, buf, len);
  }}
  else if (strcmp (cmd, "i2c_write") == 0) { CHKARGS (2) {
    int	i2c_addr = strtol (argv[optind], 0, 0);
    int	len=0;
    char *hex_string  = argv[optind + 1];
    unsigned char *buf = hex_string_to_binary (hex_string, &len);
    if (buf == 0)
      chk_result (0);

    bool result = usrp_i2c_write (udh, i2c_addr, buf, len);
    chk_result (result);
  }}
#ifdef UNHACKED_USRP
  else if (strcmp (cmd, "9862a_write") == 0) { CHKARGS (2) {
    int regno = strtol (argv[optind], 0, 0);
    int value = strtol (argv[optind+1], 0, 0);
    chk_result (usrp_9862_write (udh, 0, regno, value));
  }}
  else if (strcmp (cmd, "9862b_write") == 0) { CHKARGS (2) {
    int regno = strtol (argv[optind], 0, 0);
    int value = strtol (argv[optind+1], 0, 0);
    chk_result (usrp_9862_write (udh, 1, regno, value));
  }}
  else if (strcmp (cmd, "9862a_read") == 0) { CHKARGS (1) {
    int regno = strtol (argv[optind], 0, 0);
    unsigned char value;
    bool result = usrp_9862_read (udh, 0, regno, &value);
    if (!result){
      chk_result (0);
    }
    fprintf (stdout, "reg[%d] = 0x%02x\n", regno, value);
  }}
  else if (strcmp (cmd, "9862b_read") == 0) { CHKARGS (1) {
    int regno = strtol (argv[optind], 0, 0);
    unsigned char value;
    bool result = usrp_9862_read (udh, 1, regno, &value);
    if (!result){
      chk_result (0);
    }
    fprintf (stdout, "reg[%d] = 0x%02x\n", regno, value);
  }}
#endif
#if 0
  else if (strcmp (cmd, "one_shot") == 0) { CHKARGS (0) {
    chk_result (usrp_one_shot (udh));
  }}
  else if (strcmp (cmd, "test_busif") == 0) { CHKARGS (0) {
    chk_result (usrp_test_busif (udh));
  }}
  else if (strcmp (cmd, "test_output") == 0) { CHKARGS (0) {
    chk_result (usrp_test_output (udh));
  }}
  else if (strcmp (cmd, "test_input") == 0) { CHKARGS (0) {
    chk_result (usrp_test_input (udh, verbose));
  }}
#endif
  else if (strcmp (cmd, "jtag_opts") == 0) { CHKARGS (1) {
    char *p = argv[optind];
    unsigned char buf[16];

    memset (buf, '0', 16);
    for (int i = 0; i < 16 && *p && *(p+1); i++) {
	      buf[i] = (from_hex(*p)<<4) + from_hex(*(p+1));
	      p+=2;
    }
 
    chk_result (usrp_set_jtag_opts(udh, buf));
  }}
  else {
    usage ();
  }

  if (udh){
    usrp_close_interface (udh);
    udh = 0;
  }

  return 0;
}


#if 0
/*
 * write infinite amount of test data to FPGA
 */

bool 
usrp_test_output (struct usb_dev_handle *udh)
{
  static const int MAX_N = 512 / sizeof (short) * 32;
  unsigned short buf[MAX_N];
  
  // FIXME reset FPGA, enable TX

  int	n = MAX_N;		// if we enumerate as full speed, this should be 64/2
  //int	counter = ~0;
  int	counter = 0;
  int	loop_counter = 0;
  
  while (1){
    loop_counter++;
    
    // fill in buffer with sequence of shorts
    for (int i = 0; i < n; i++){
      buf[i] = counter;
      //counter--;
      counter++;
    }

    int r = usb_bulk_write (udh, USRP_DATAOUT_ENDPOINT, (char *) buf, n * sizeof (short), 1000);
    if (r < 0){
      fprintf (stderr, "usb_bulk_write failed (loop %d): %s\n", loop_counter, usb_strerror ());
      return false;
    }
  }

  return true;			// not reached
}
  
/*
 * read infinite amount of data from FPGA
 */

bool 
usrp_test_input (struct usb_dev_handle *udh, bool write_file)
{
  static const int MAX_N = 512 / sizeof (short) * 32;
  unsigned short buf[MAX_N];
  
  FILE *fp = 0;
  char *filename = "test_input.dat";
  
  if (write_file){
    fp = fopen (filename, "wb");
    if (fp == 0)
      perror (filename);
  }

  usrp_set_fpga_rx_enable (udh, false);
  usrp_set_fpga_tx_enable (udh, false);
  usrp_set_fpga_reset (udh, true);
  usrp_set_fpga_reset (udh, false);

  unsigned int adc_clk_div =   2;
  unsigned int ext_clk_div =  12;
  unsigned int interp_rate = 255;
  unsigned int decim_rate  =   1;
  
  unsigned int	v = ((adc_clk_div << 24) | (ext_clk_div << 16)
		     | (interp_rate << 8) | (decim_rate));

  usrp_write_fpga_reg (udh, 8, v);
  usrp_set_fpga_rx_enable (udh, true);
  

  int	n = MAX_N;		// if we enumerate as full speed, this should be 64/2
  int	loop_counter = 0;
  
  while (1){
    loop_counter++;
    
    int r = usb_bulk_read (udh, USRP_DATAIN_ENDPOINT, (char *) buf,
			   n * sizeof (short), 1000);
    if (r < 0){
      fprintf (stderr, "usb_bulk_read failed (loop %d): %s\n",
	       loop_counter, usb_strerror ());
      return false;
    }

    if (fp)
      fwrite (buf, 1, n * sizeof (short), fp);
  }

  return true;			// not reached
}
#endif

#if 0
/*
 * diagnostic routine.  This requires that the
 * passthru bitsteam is loaded into the fpga.
 *
 * We reset the fpga, disable tx, then send a single packet
 * with known contents.  After waiting for user input,
 * we enable tx, which should send the know data to the dac port
 * which we can watch with the logic analyzer.
 */

bool 
usrp_one_shot (struct usb_dev_handle *udh)
{
  static const int PKTSIZE = 256 * 4;
  short buf[PKTSIZE];
  
  usrp_set_fpga_tx_enable (udh, 0);	// disable tx
  usrp_set_fpga_reset (udh, 1);		// reset fpga, emptying internal fifos
  usrp_set_fpga_reset (udh, 0);		// re-enable fpga

  
  int	n = 0;
  for (int i = 0; i < PKTSIZE / 2; i++){
    int		v;

    v = (i + 0x200) << 2;

    buf[n+0] = v;
    buf[n+1] = 0;
    n += 2;
  }

  int r = usb_bulk_write (udh, USRP_DATAOUT_ENDPOINT,
			  (char *) buf, sizeof (buf), 1000);
  if (r < 0){
    fprintf (stderr, "usb_bulk_write failed: %s\n", usb_strerror ());
    return false;
  }

#if 0
  fprintf (stdout, "Press enter to send packet: ");
  fflush (stdout);
  fgetc (stdin);
#endif

  usrp_set_fpga_tx_enable (udh, 1);

  return true;
}

bool 
usrp_test_busif (struct usb_dev_handle *udh)
{
  static const int PKTSIZE = 256 * 4;
  short buf[PKTSIZE];
  
  int	n = 0;
  for (int i = 0; i < PKTSIZE / 2; i++){
    int		v;

    v = (i + 0x200) << 2;

    buf[n+0] = v;
    buf[n+1] = 0;
    n += 2;
  }


  usrp_one_shot (udh);	// launch first burst

  while (1){

    int r = usb_bulk_write (udh, USRP_DATAOUT_ENDPOINT,
			    (char *) buf, sizeof (buf), 1000);
    if (r < 0){
      fprintf (stderr, "usb_bulk_write failed: %s\n", usb_strerror ());
      return false;
    }
  }

  return true;
}

#endif
