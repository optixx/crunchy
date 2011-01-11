/* -*- c++ -*- */
/*
 * Copyright 2003 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>			/* needed for usb functions */
#include <getopt.h>
#include <assert.h>
#include <math.h>
#include "time_stuff.h"
/* #include "usrp_standard.h" */
#include "usrp_basic.h"
#include "usrp_bytesex.h"

char *prog_name;

static bool test_input  (usrp_basic_rx *urx, int max_bytes, FILE *fp);

static void
set_progname (char *path)
{
  char *p = strrchr (path, '/');
  if (p != 0)
    prog_name = p+1;
  else
    prog_name = path;
}

static void
usage ()
{
  fprintf (stderr, "usage: %s [-f] [-v] [-l] [-c] [-D <decim>] [-F freq] [-o output_file]\n", prog_name);
  fprintf (stderr, "  [-w which] board select\n");
  fprintf (stderr, "  [-f] loop forever\n");
  fprintf (stderr, "  [-M] how many Megabytes to transfer (default 128)\n");
  fprintf (stderr, "  [-v] verbose\n");
  fprintf (stderr, "  [-l] digital loopback in FPGA\n");
  fprintf (stderr, "  [-c] counting in FPGA\n");
  exit (1);
}

static void
die (const char *msg)
{
  fprintf (stderr, "die: %s: %s\n", prog_name, msg);
  exit (1);
}

int
main (int argc, char **argv)
{
  bool 	verbose_p = false;
  bool	loopback_p = false;
  bool  counting_p = false;
  int   max_bytes = 128 * (1L << 20);
  int	ch;
  char	*output_filename = 0;
  int	which_board = 0;
  int	decim = 8;			// 32 MB/sec
  double	center_freq = 0;

  set_progname (argv[0]);

  while ((ch = getopt (argc, argv, "fw:vlco:D:F:M:")) != EOF){
    switch (ch){
    case 'f':
      max_bytes = 0;
      break;

    case 'w':
      which_board = atoi(optarg);
      break;

    case 'v':
      verbose_p = true;
      break;

    case 'l':
      loopback_p = true;
      break;

    case 'c':
      counting_p = true;
      break;
      
    case 'o':
      output_filename = optarg;
      break;
      
    case 'D':
      decim = strtol (optarg, 0, 0);
      break;

    case 'F':
      center_freq = strtod (optarg, 0);
      break;

    case 'M':
      max_bytes = strtol (optarg, 0, 0) * (1L << 20);
      if (max_bytes < 0) max_bytes = 0;
      break;

    default:
      usage ();
    }
  }

  
  FILE *fp = 0;

  if (output_filename){
    fp = fopen (output_filename, "wb");
    if (fp == 0)
      perror (output_filename);
  }

#if 0
  int mode = 0;
  if (loopback_p)
    mode |= usrp_standard_rx::FPGA_MODE_LOOPBACK;
  if (counting_p)
    mode |= usrp_standard_rx::FPGA_MODE_COUNTING;
#endif


  usrp_basic_rx *urx = usrp_basic_rx::make (which_board);
  if (urx == 0)
    die ("usrp_basic_rx::make");

#if 0
  if (!urx->set_rx_freq (0, center_freq))
    die ("urx->set_rx_freq");
#endif

  urx->start();		// start data xfers

  test_input (urx, max_bytes, fp);

  if (fp)
    fclose (fp);

  delete urx;

  return 0;
}


static bool
test_input  (usrp_basic_rx *urx, int max_bytes, FILE *fp)
{
  int		   fd = -1;
  static const int BUFSIZE = urx->block_size();
  static const int N = BUFSIZE/sizeof (short);
  short 	   buf[N];
  int		   nbytes = 0;

  double	   start_wall_time = get_elapsed_time ();
  double	   start_cpu_time  = get_cpu_usage ();

  if (fp)
    fd = fileno (fp);
  
  bool overrun;
  int noverruns = 0;

  for (nbytes = 0; max_bytes == 0 || nbytes < max_bytes; nbytes += BUFSIZE){

    unsigned int	ret = urx->read (buf, sizeof (buf), &overrun);
    if (ret != sizeof (buf)){
      fprintf (stderr, "test_input: error, ret = %d\n", ret);
    }

    if (overrun){
      printf ("rx_overrun\n");
      noverruns++;
    }
    
    if (fd != -1){

      for (unsigned int i = 0; i < sizeof (buf) / sizeof (short); i++)
	buf[i] = usrp_to_host_short (buf[i]);

      if (write (fd, buf, sizeof (buf)) == -1){
	perror ("write");
	fd = -1;
      }
    }
  }

  double stop_wall_time = get_elapsed_time ();
  double stop_cpu_time  = get_cpu_usage ();

  double delta_wall = stop_wall_time - start_wall_time;
  double delta_cpu  = stop_cpu_time  - start_cpu_time;

  printf ("xfered %.3g bytes in %.3g seconds.  %.4g bytes/sec.  cpu time = %.4g\n",
	  (double) max_bytes, delta_wall, max_bytes / delta_wall, delta_cpu);
  printf ("noverruns = %d\n", noverruns);

  return true;
}
