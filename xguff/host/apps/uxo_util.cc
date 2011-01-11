// debug printf's
#include <stdio.h>

#include "uxo_util.h"
#include "usrp_prims.h"

void write_fpga_reg(struct usb_dev_handle *udh, int add, int val) {
	// printf("write_fpga_reg %d %d\n", add, val);
	if (udh) usrp_write_fpga_reg (udh, add, val);
}

struct usb_dev_handle *setup_uxo(int which_board)
{

	bool fx2_ok_p = false;
	struct usb_dev_handle *udh = 0;
	struct usb_device *udev = usrp_find_device (which_board, fx2_ok_p);
	if (udev == 0 || usrp_unconfigured_usrp_p(udev) || usrp_fx2_p (udev)) {
		fprintf(stderr, "couldn't connect to working device\n");
		udev = 0;
	} else if ((udh = usrp_open_cmd_interface (udev)) == 0) {
		fprintf (stderr, "failed to open_cmd_interface\n");
		udev = 0;
	}
	return udh;
}

int get_wave(fusb_ephandle *d_ephandle, short *dest, unsigned long max_bytes)
{
	static const int N = 8 * 1024;
	static const int BUFSIZE = N * sizeof (short);
	bool overrun=0;
	int noverruns = 0;
	d_ephandle->start();
	for (unsigned long nbytes = 0; nbytes < max_bytes; nbytes += BUFSIZE){
		long int to_read = max_bytes-nbytes;
		if (to_read > BUFSIZE) to_read = BUFSIZE;
		long int ret = d_ephandle->read (dest, to_read);
		if (ret != to_read){
			fprintf (stderr, "test_input: error, ret = %ld\n", ret);
		}
		dest += N;
		if (overrun){
			noverruns++;
		}
	}
	d_ephandle->stop();
	if (noverruns > 0) {
		fprintf (stderr, "test_input: noverruns = %d\n", noverruns);
	}
	return 0;
}

int uxo_take_data(struct usb_dev_handle *udh, short *data, int data_length, int acq_mode)
{
	static const int FUSB_BLOCK_SIZE = 16 * (1L << 10);       // 16 KB
	static const int FUSB_BUFFER_SIZE = 8 * (1L << 20);       // 8 MB
	static const int FUSB_NBLOCKS    = FUSB_BUFFER_SIZE / FUSB_BLOCK_SIZE;

	fprintf(stderr,"take_data start\n");
	if (udh == 0) {fprintf(stderr,"take_data abort 1\n");return 1;}

	fusb_devhandle *d_devhandle = fusb_sysconfig::make_devhandle (udh);
	if (d_devhandle == 0) {fprintf(stderr,"take_data abort 2\n");return 1;}
	
	fusb_ephandle *d_ephandle = d_devhandle->make_ephandle (6, true, FUSB_BLOCK_SIZE, FUSB_NBLOCKS);
	if (d_ephandle == 0) {fprintf(stderr,"take_data abort 3\n");return 1;}

	write_fpga_reg(udh, 4, 0x08);  // reset FPGA RX FIFO
	usrp_set_fpga_rx_reset(udh, true);   // reset FX2 FIFO
	usrp_set_fpga_rx_reset(udh, false);  // reset FX2 FIFO
	usrp_set_fpga_rx_enable(udh, true);
	write_fpga_reg(udh, 4, 0x02);  // FPGA FIFO operate
	usb_resetep(udh, 6);
	write_fpga_reg(udh, 70, acq_mode);    // turn on data source
	get_wave(d_ephandle, data, data_length);
	usrp_set_fpga_rx_enable(udh, false);
	write_fpga_reg(udh, 4, 0x08);  // reset FIFO
	write_fpga_reg(udh, 70, 1);    // turn off data source
	fprintf(stderr,"take_data complete\n");
	return 0;
}
