#ifndef UXO_UTIL_H
#define UXO_UTIL_H 1

#include <usb.h>
#include "fusb.h"

void write_fpga_reg(struct usb_dev_handle *udh, int add, int val);
struct usb_dev_handle *setup_uxo(int which_board);
int get_wave(fusb_ephandle *d_ephandle, short *dest, unsigned long max_bytes);
int uxo_take_data(struct usb_dev_handle *udh, short *data, int data_length, int acq_mode);

#endif
