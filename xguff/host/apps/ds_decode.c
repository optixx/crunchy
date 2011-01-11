#include <stdio.h>
#include <string.h>

/* Dallas 1-Wire[TM] device support routines adapted from llc-suite
 *  http://recycle.lbl.gov/~ldoolitt/llc-suite/
 * Copyright (c) 2004, The Regents of the University of California
 */
static unsigned int compute_onewire_crc(unsigned int length, unsigned char data[])
{
	unsigned int i, d, b, crc;
	crc = 0;
	for (i=0; i<length; i++) {
		unsigned int j;
		d = data[length-1-i];
		for (j=0; j<8; j++) {
			b = d & 1;
			if ((crc&1) ^ b) crc ^= 0x118;
			crc = crc>>1;
			d = d>>1;
		}
	}
	return crc;
}

int get_dallas_temperature(unsigned char scratch[9], double *temperature)
{
	unsigned int crc, tempu;
	double temp;
	crc = compute_onewire_crc(9, scratch);
	tempu = ((unsigned int)scratch[7]<<8)+(unsigned int)scratch[8];
	if (tempu & 0x8000U) tempu |= ~0xffU;  /* sign-extend */
	temp = ((signed int)tempu)*0.0625;
	if (crc == 0 && temperature) *temperature = temp;
	return (crc==0);
}

int print_dallas_results(int quiet,  unsigned char id[8], FILE *f)
{
	unsigned int i, crc;

	crc = compute_onewire_crc(8, id);

	if (quiet > 1) {
		/* don't print anything */
	} else if (quiet) {
		if (crc==0) {
			for (i=1; i<7; i++) {
				fprintf(f, "%.2x", (unsigned int) id[i]);
			}
		} else {
			fprintf(f, "unknown");
		}
		putc('\n',f);
	} else {
		/* reference: table in "The 1-Wire API for Java",
		 * and tables 1 and 4 of Maxim app note 155 */
		const char *name = NULL;
		switch(id[7]) {
		case 0x01: name="DS2401" ; break;  /* or DS1990A */
		case 0x02: name="DS1425" ; break;  /* or DS1991 or DS1427 */
		case 0x04: name="DS2404" ; break;  /* or DS1994 */
		case 0x05: name="DS2405" ; break;
		case 0x06: name="DS1993" ; break;
		case 0x08: name="DS1992" ; break;
		case 0x09: name="DS2502" ; break;  /* or DS1982 */
		case 0x0A: name="DS1995" ; break;
		case 0x0B: name="DS2505" ; break;  /* or DS1985 */
		case 0x0C: name="DS1996" ; break;  /* or DS1996x2 or DS1996x4 */
		case 0x0F: name="DS2506" ; break;  /* or DS1986 */
		case 0x10: name="DS1820" ; break;  /* or DS1920 or DS18S20 */
		case 0x12: name="DS2406" ; break;  /* or DS2407 */
		case 0x13: name="DS2503" ; break;  /* or DS1983 */
		case 0x14: name="DS2430A"; break;  /* or DS1971 */
		case 0x16: name="DS1954" ; break;
		case 0x18: name="DS1963S"; break;
		case 0x1A: name="DS1963L"; break;
		case 0x1D: name="DS2423" ; break;
		case 0x1F: name="DS2409" ; break;
		case 0x20: name="DS2450" ; break;
		case 0x21: name="DS1921" ; break;  /* or DS1921H or DS1921Z */
		case 0x22: name="DS1822" ; break;
		case 0x23: name="DS2433" ; break;  /* or DS1973 */
		case 0x24: name="DS2415" ; break;  /* or DS1904 */
		case 0x26: name="DS2438" ; break;
		case 0x27: name="DS2417" ; break;
		case 0x28: name="DS18B20"; break;
		case 0x2C: name="DS2890" ; break;
		case 0x30: name="DS2760" ; break;
		case 0x33: name="DS2432" ; break;  /* or DS1961S */
		case 0x51: name="DS2751" ; break;
		case 0x91: name="DS1981" ; break;
		case 0x96: name="DS1955" ; break;  /* or DS1957 or DS1957B */
		default: break;
		}
		if (name==NULL) {
			fprintf(f, "DOW-%.2x: ", (unsigned int) id[7]);
		} else {
			fprintf(f, "%s: ", name);
		}
		for (i=1; i<7; i++) {
			fprintf(f, "%.2x", (unsigned int) id[i]);
		}
		fprintf(f, "  CRC: %.2x", (unsigned int) id[0]);
		if (crc==0) {
			fprintf(f, " OK");
		} else {
			fprintf(f, " FAIL-%.2x", crc);
		}
		putc('\n',f);
		fflush(f);
	}
	return crc ? 3 : 0;
}

/* returns 1 for success, 0 for failure */
int ponder_dallas(unsigned int seq, unsigned int house, unsigned char *id[8], double *temperature)
{
	static unsigned char dallas[32];
	static int state=0;
	unsigned int ix=31-seq;
	if (state==0) memset(dallas, 0, 32);
	if (seq==0) state=1;
	if (state!=0 && house!=dallas[ix]) {
		dallas[ix] = house;
		state=2;
	}
	if (state==2 && seq==31 && dallas[28]==0xcc) {
		*id=dallas+4;
		get_dallas_temperature(dallas+18, temperature);
		state=1;
		return 1;
	}
	return 0;  /* not done yet */
}

#if 0
void print_dallas(const unsigned char *data)
{
	unsigned char *id;
	double temperature;
	unsigned int start=0;
again:
	start = fish_dallas_extra(data, start, &id, &temperature);
	if (start > 0) {
		print_dallas_results(0, id);
		printf("temperature: %.2f C\n", temperature);
		/* If you only need one result per data file, don't loop.
		 */
		goto again;
	}
}
#endif
