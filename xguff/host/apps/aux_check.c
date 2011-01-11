#include <stdio.h>

#define MAKE_LONG(x) (*(x)|*((x)+1)<<8 | *((x)+2)<<16 | *((x)+3)<<24)
#define MAKE_SHORT(x) (*(x)|*((x)+1)<<8)

/* skip the header file for now */
int ponder_dallas(unsigned int seq, unsigned int house, unsigned char *id[8], double *temperature);
int print_dallas_results(int quiet,  unsigned char id[8], FILE *f);

void print_config(const unsigned char *data, FILE *print)
{
	unsigned int seq, house;
	const char *months[]={ "???",
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	for (seq=0; seq<16; seq++) {
		house=data[seq];
		switch (seq) {
		 case  0: if (house != 0x55) goto fail; break;
		 case  1: if (house != 0x01) goto fail; fprintf(print, "LLRF "   ); break;
		 case  2: fprintf(print,"build %d-", house+2000); break; /* year */
		 case  3: fprintf(print,"%s-", (house < 13) ? months[house] : "???"); break; /* month */
		 case  4: fprintf(print,"%2.2d ",       house     ); break; /* day */
		 case  5: fprintf(print,"%2.2d:",       house     ); break; /* hour */
		 case  6: fprintf(print,"%2.2d:00 UTC, ",    house     ); break; /* minute */
		 case  7: fprintf(print,"using %s-", house==2 ? "XST" : "unknown"); break;
		 case  8: fprintf(print,"%2x ", house); break;  /* version */
		 case  9: /* printf("%d ", house); */ break;   /* user */
		 case 10: fprintf(print,"for %s board", (house==4) ? "Avnet" :
		                                 (house==5) ? "UXO Integrated" :
		                                 (house==6) ? "LLRF4" : "unknown");
		 default: fprintf(print,"\n"); return;
		}
	}
fail:	fprintf(print, "FAIL\n");
	return;
}

void aux_process_buff(unsigned char buff[256], FILE *print, unsigned *burst_age)
{
	static unsigned char prev_print[256];
	static unsigned int next_seq = -1;
	static unsigned int adc_hold[8]={-1,-1,-1,-1,-1,-1,-1,-1};
	static unsigned int adc_up[8]={0,0,0,0,0,0,0,0};
	static int printed_config = 0;
	unsigned u;

	if (printed_config == 0) {
		fprintf(print, "s0 ");
		print_config(buff+16, print);
		printed_config = 1;
	}

	unsigned seq = MAKE_LONG(buff);
	if (seq != next_seq && next_seq != -1U) {
		printf("secondary sequence error: %8.8x != %8.8x\n", seq, next_seq);
	}
	next_seq = (seq+1) & 0xffffffff;

	if (burst_age) *burst_age = (buff[0x00] - buff[0x71])*256 + 0xff - buff[0x70];

	unsigned int vadc = buff[64] | (buff[80]&0x0f) <<8;
	unsigned int nadc = (buff[80]&0x70)>>4;
	if (0) printf("adc debug %2x %3x %x %3x\n", buff[64], buff[80], nadc, vadc);
	if (adc_hold[nadc] != vadc) {
		adc_hold[nadc] = vadc;
		adc_up[nadc] = 1;
	}
	if (print) {
		fprintf(print, "s1 %d\n", seq);
		for (u=0; u<8; u++) {
			if (adc_up[u]) fprintf(print, "a%u %u\n", u, adc_hold[u]);
			adc_up[u] = 0;
		}
		for (u=0; u<4; u++) {
			if (0) {
				long ff=MAKE_LONG(buff+0xe0+u*4);
				fprintf(print, "A%u %+7.4f\n", u, (double)ff/2147483648.0);
			} else {
				short ff1=MAKE_SHORT(buff+0xe0+u*4);
				short ff2=MAKE_SHORT(buff+0xe2+u*4);
				fprintf(print, "A%u %+7.4f %+7.4f\n", u,
					 (double)ff1/32768.0, (double)ff2/32768.0);
			}
		}
		unsigned int ofreq = MAKE_LONG(prev_print+96);
		unsigned int nfreq = MAKE_LONG(buff+96);
		/* DSP frequency = nfreq * 48 MHz / 2^24 */
		if (nfreq != ofreq) fprintf(print, "s2 %.3f MHz\n", nfreq * (48.0/16777216.0));
		for (u=0; u<256; u++) prev_print[u] = buff[u];
	}
}

void aux_check_word(unsigned int v, FILE *print, unsigned *burst_age)
{
	static unsigned int hope_cnt = -1;
	static unsigned char save[256];
	unsigned char *dallas_id;
	double dallas_temperature;
	static int state = 0;
	static int errors = 0;
	static int dallas_once = 0;
	if (0) printf("input 0x%4.4x\n", v);

	unsigned int cnt = v & 0xff;
	if (hope_cnt == -1U) hope_cnt = cnt;
	if (hope_cnt != cnt) {
		errors++;
		state=0;
	}

	save[cnt] = v>>8;
	if (cnt==0) state=1;
	else if (state==1 && cnt==255) aux_process_buff(save,print,burst_age);
	if ((cnt&0xe0) == 0x20 && ponder_dallas(cnt&0x1f, v>>8, &dallas_id, &dallas_temperature)) {
		if (dallas_once == 0) {
			fprintf(print, "s3 ");
			print_dallas_results(0, dallas_id, print);
		}
		fprintf(print, "a8 %.2f C\n", dallas_temperature);
		dallas_once = 1;
	}

	hope_cnt = (cnt+1) & 0xff;
}

#ifdef STANDALONE
int main(void)
{
	unsigned int d;
	while (fscanf(stdin,"0x%x data\n",&d)==1) aux_check_word(d,stdout,NULL);
	return 0;
}
#endif
