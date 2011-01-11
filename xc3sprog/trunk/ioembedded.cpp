/* JTAG GNU/Linux parport device io

Copyright (C) 2010 Felix Domke

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "ioembedded.h"
#include "gpio.h"

using namespace std;


IOEmbedded::IOEmbedded() : IOBase(), total(0) {
	printf("open gpio\n");
	xgpio_init();
	printf("init default gpio\n");
	xgpio_set_oe(GPIO_TDI, 1);
	xgpio_set_oe(GPIO_TDO, 0);
	xgpio_set_oe(GPIO_TMS, 1);
	xgpio_set_oe(GPIO_TCK, 1);
	printf("ok\n");
}

bool IOEmbedded::txrx(bool tms, bool tdi)
{
	unsigned char ret;
	bool retval;
	unsigned char data=0;
	if(tdi)data|=TDI;
	if(tms)data|=TMS;
	set_io(data);
	data|=TCK;
	retval = set_io(data);
	total++;
	return retval; 
		
}

void IOEmbedded::tx(bool tms, bool tdi)
{
	unsigned char data=0; // D4 pin5 TDI enable
	if(tdi)data|=TDI; // D0 pin2
	if(tms)data|=TMS; // D2 pin4
	set_io(data);
	data|=TCK; // clk high 
	total++;
	set_io(data);
}
 
void IOEmbedded::tx_tdi_byte(unsigned char tdi_byte)
{
	int k;
	
	for (k = 0; k < 8; k++)
		tx(false, (tdi_byte>>k)&1);
}
 
void IOEmbedded::txrx_block(const unsigned char *tdi, unsigned char *tdo,
													 int length, bool last)
{
	int i=0;
	int j=0;
	unsigned char tdo_byte=0;
	unsigned char tdi_byte;
	unsigned char data=0;
	if (tdi)
			tdi_byte = tdi[j];
			
	while(i<length-1){
			tdo_byte=tdo_byte+(txrx(false, (tdi_byte&1)==1)<<(i%8));
			if (tdi)
					tdi_byte=tdi_byte>>1;
		i++;
		if((i%8)==0){ // Next byte
				if(tdo)
						tdo[j]=tdo_byte; // Save the TDO byte
			tdo_byte=0;
			j++;
			if (tdi)
					tdi_byte=tdi[j]; // Get the next TDI byte
		}
	};
	tdo_byte=tdo_byte+(txrx(last, (tdi_byte&1)==1)<<(i%8)); 
	if(tdo)
			tdo[j]=tdo_byte;
	set_io(data); /* Make sure, TCK is low */
	return;
}

void IOEmbedded::tx_tms(unsigned char *pat, int length, int force)
{
	int i;
	unsigned char tms;
	unsigned char data=0;
	for (i = 0; i < length; i++)
	{
		if ((i & 0x7) == 0)
			tms = pat[i>>3];
		tx((tms & 0x01), true);
		tms = tms >> 1;
	}
	set_io(data); /* Make sure, TCK is low */
}

IOEmbedded::~IOEmbedded()
{
	if (verbose) fprintf(stderr, "Total bytes sent: %d\n", total>>3);
}


int IOEmbedded::set_io(unsigned char data)
{
	xgpio_set(GPIO_TDI, !!(data & TDI));
	xgpio_set(GPIO_TMS, !!(data & TMS));
	xgpio_set(GPIO_TCK, !!(data & TCK));
	return xgpio_get(GPIO_TDO);
}
