/* Flag string for output file: */
#define JDRIVE_VERSION "3.5"

/* Defects:
 *   filenames control.in and temp are hard coded
 *   excite_pattern command is accepted but ignored, its default
 *     of 5 is not changeable.
 *
 * Other usage notes:
 *   for default configuration, search source code for "default configuration"
 *   sim mode is artificially slowed down by a factor of 5
 *
 * Dump data that junpack refuses to look at:
 *   hexdump -e '16/2 "%04X " "\n"' utest1_0086.bin
 *
 * Debug excitation sequence
 *   hexdump -e '16/2 "%04X " "\n"' utest1.bin | awk '{if ($10!=o) {print o,c;c=0} o=$10;c++}'
 * 02 08 90 02 84 20 81 08
 *
 * 0220 8160   0220*54  0820
 * 
 */
#define _POSIX_C_SOURCE 199309
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#define LIVE
#ifdef LIVE
#include "uxo_util.h"
#include "usrp_prims.h"
#endif

#if 0
#define DPRINTF(format, args...)  fprintf(stderr, format, ##args)
#else
#define DPRINTF(format, args...)
#endif

char *get_param(const char *s)
{
	size_t l;
	char *b = strdup(s);
	if (!b) {perror("strdup"); return NULL;}
	/* chop off a trailing \n, if any */
	l = strlen(b);
	if (l>0 && *(b+l-1)=='\n') { *(b+l-1)='\0'; }
	return b;
}

struct roll_config {

	const char *command_file;
	const char *meta_file;
	char *meta_file_new;
	int mode;
	int excite_cycles;
	int excite_pattern;
	int local_stack;
	char *output_filebase;
	char *output_single;
	FILE *fhand;
	char *actual_file;
	int cycles_handled;
	int param_changed;

	int fcount;
	char *gps_gga_before;
	char *gps_ring;
	char *odom_zero;

	int odom_out[5];
	int odom_print;
	int sadc_print;

	int test_stack;
	int adc_period;
	int excite_per;
	int extra_per;
	int gain_switch;
	int cnv_pos;
	int xmit_pulse;
	int force_low_gain;
	int odom_xor;
#ifdef LIVE
	usb_dev_handle *udh;
#endif
};

int acq_mode(struct roll_config *cfg)
{
	return cfg->local_stack ? 11 : 7;
}

typedef void (*action_routine)(const char *, struct roll_config *);

static void do_pause(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	DPRINTF("do_pause (%s)\n", s);
	c->mode = 0;
}

static void do_run(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	DPRINTF("do_run (%s)\n", s);
	c->mode = 1;
}

static void do_single(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	DPRINTF("do_single (%s)\n", s);
	c->mode = 2;
}

static void do_excite_cycles(const char *s, struct roll_config *c)
{
	DPRINTF("do_excite_cycles (%s)\n", s);
	c->excite_cycles = atoi(s);
}

static void do_excite_pattern(const char *s, struct roll_config *c)
{
	DPRINTF("do_excite_pattern (%s)\n", s);
	/* disabled since data alignment code only supports mode 5 */
	int n = atoi(s);
	if (n>=0 && n<=5 && n!=4) {
		if (c->excite_pattern != n) c->param_changed = 1;
		c->excite_pattern = n;
	}
}

static void do_odom_zero(const char *s, struct roll_config *c)
{
	DPRINTF("do_odom_zero (%s)\n", s);
	char *b = get_param(s);
	if (c->odom_zero) {
		free(c->odom_zero);
		c->odom_zero = 0;
	}
	c->odom_zero = b;
}

static void do_output_filebase(const char *s, struct roll_config *c)
{
	DPRINTF("do_output_filebase (%s)\n", s);
	char *b = get_param(s);
	if (!b) return;

	if (c->output_filebase==NULL || strcmp(b, c->output_filebase)!=0) {
		c->fcount = 1;
	}
	if (c->output_filebase) free(c->output_filebase);
	c->output_filebase = b;
	c->mode = 1;   /* run */
}

static void do_output_single(const char *s, struct roll_config *c)
{
	DPRINTF("do_output_single (%s)\n", s);
	char *b = get_param(s);
	if (!b) return;

	if (c->output_single==NULL || strcmp(b, c->output_single)!=0) {
		c->mode=2;   /* single */
	}
	if (c->output_single) free(c->output_single);
	c->output_single = b;
}

static void do_exit(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	(void) c; /* unused */
	DPRINTF("do_exit (%s)\n", s);
	exit(0);
}

#define CMD(x) { #x, do_ ## x}
static struct {
	const char *command;
	action_routine action;
} cmd_table[] = {
	CMD(pause),
	CMD(run),
	CMD(single),
	CMD(excite_cycles),
	CMD(excite_pattern),
	CMD(odom_zero),
	CMD(output_filebase),
	CMD(output_single),
	CMD(exit)
};
#undef CMD

#define ENTRY(x) { #x, offsetof(roll_config, x)}
static struct {
	const char *command;
	unsigned offset;
} par_table[] = {
	ENTRY(local_stack),
	ENTRY(test_stack),
	ENTRY(adc_period),
	ENTRY(excite_per),
	ENTRY(gain_switch),
	ENTRY(cnv_pos),
	ENTRY(xmit_pulse),
	ENTRY(force_low_gain),
	ENTRY(odom_xor)
};
#undef ENTRY
#define FIELD(s,o) (*(int *)((char *)(s)+o))

void jdrive_cmd(const char *cmd, struct roll_config *cfg)
{
	unsigned u;
	for (u=0; u<sizeof cmd_table/sizeof *cmd_table; u++) {
		const char *check = cmd_table[u].command;
		if (strncmp(cmd, check, strlen(check))==0) {
			const char *s=cmd+strlen(check);
			if (*s==':') s++;
			while (isspace(*s)) {s++;}
			cmd_table[u].action(s, cfg);
			/* printf("ready\n");  fflush(stdout); */
			return;
		}
	}
	for (u=0; u<sizeof par_table/sizeof *par_table; u++) {
		const char *check = par_table[u].command;
		if (strncmp(cmd, check, strlen(check))==0) {
			long val;
			const char *s=cmd+strlen(check);
			if (*s==':') s++;
			while (isspace(*s)) {s++;}
			val = strtol(s, NULL, 0);
			if (FIELD(cfg,par_table[u].offset) != val) {
				FIELD(cfg,par_table[u].offset) = val;
				cfg->param_changed = 1;
			}
			return;
		}
	}
	DPRINTF("unhandled %s\n",cmd);
}

void uxo_write_header(FILE *f, size_t length, struct roll_config *cfg)
{
	int adc_channels=16;
	/* gettimeofday() is not standard */
	/* clock_gettime needs USE_POSIX199309 and -lrt */
	struct timespec now;
	fprintf(f,"jdrive_version: " JDRIVE_VERSION "\n");
	if (clock_gettime(CLOCK_REALTIME, &now)<0) {
		perror("clock_gettime");
	} else {
		struct tm bar;
		gmtime_r(&(now.tv_sec),&bar);
		char *foo=asctime(&bar);
		fprintf(f,"utc_date_stamp: %s", foo);
		fprintf(f,"utc_unix: %lu\n", now.tv_sec);
		fprintf(f,"utc_unix_ms: %lu\n", now.tv_nsec/1000000);
	}
	fprintf(f,"pulse_number: %d\n", 1);
	fprintf(f,"adc_channels: %d\n", adc_channels);
	fprintf(f,"sample_count: %zd\n", length);

#define ENTRY(x) fprintf(f,#x ": %d\n", cfg->x)
	ENTRY(local_stack);
	ENTRY(test_stack);
	ENTRY(excite_cycles);
	ENTRY(adc_period);
	ENTRY(excite_per);
	ENTRY(extra_per);
	ENTRY(gain_switch);
	ENTRY(excite_pattern);
	ENTRY(cnv_pos);
	ENTRY(xmit_pulse);
	ENTRY(force_low_gain);
	ENTRY(odom_xor);
#undef ENTRY
	fprintf(f, "odometer: %d %d %d %d %d\n",
		cfg->odom_out[0],
		cfg->odom_out[1],
		-cfg->odom_out[2],
		cfg->odom_out[3],
		cfg->odom_out[4]);
	if (cfg->odom_zero) fprintf(f,"odom_zero: %s\n", cfg->odom_zero);

	char *gps_ring = cfg->gps_ring;
#define GPS_STRING_OK(type) (gps_ring && ((gps_ring[type]&0xf0) == 0xf0))
#define GPS_STRING(type) (gps_ring+(gps_ring[type]&0x0f)*256+10)

	// jdrive2 writes the header before collecting data
	if (GPS_STRING_OK(2)) {
		fprintf(f,"gps_gga_before: %s\n", GPS_STRING(2));
	}
	if (GPS_STRING_OK(3)) {
		fprintf(f,"gps_gst: %s\n", GPS_STRING(3));
	}

	fputs("%%data%%",f);
	long p=ftell(f);
	fwrite(".........", 1, 8-(p+1)%8, f);   // pad to double-word boundary
	fputs("\n",f);
}

int count_samples(struct roll_config *cfg)
{
	int length = cfg->excite_per*6;
	if (cfg->local_stack == 0) {
		length = length*9;
	}
	length = (length+cfg->extra_per)*cfg->excite_cycles;
	return length;
}

static int rnd1(void)
{
	static unsigned int r=0;
	static unsigned int Y=1;
	if (r==0U || r==1U || r==-1U) r=333; /* keep from getting stuck */
	r = (9973 * ~r) + ((Y) % 701); /* the actual algorithm */
	Y = (r>>20) % 19; /* choose upper bits and reduce */
	return Y;
}

short *fake_data(struct roll_config *cfg, size_t *byte_length)
{
	static short *mem=0;
	static size_t max_length=0;
	unsigned int u;
	size_t length=count_samples(cfg);
	size_t bl=length*32;
	DPRINTF("mem=%p  length=%zu  max_length=%zu\n", mem, length, max_length);
	if (mem && length > max_length) mem = (short int *) realloc(mem, bl);
	if (!mem) {
		mem = (short int *) malloc(bl);
		max_length = length;
	} else if (length > max_length) {
		mem = (short int *) realloc(mem, bl);
		max_length = length;
	}
	if (!mem) return NULL;
	for (u=0; u<length; u++) {
		memset(mem+u*16,0,32);
		for (int i=0; i<12; i++) mem[u*16+ i] = rnd1()+rnd1()+rnd1()-18;
		mem[u*16+12] = u;
	}
	if (byte_length) *byte_length = bl;
	DPRINTF("returning %p\n",mem);
	return mem;
}

char *output_file_gen(struct roll_config *cfg)
{
	char *fn;
	if (cfg->mode == 1 && cfg->output_filebase) {
		fn=(char *)malloc(strlen(cfg->output_filebase)+10);
		if (fn) sprintf(fn,"%s_%4.4d.bin",cfg->output_filebase,cfg->fcount);
	} else if (cfg->mode == 2 && cfg->output_single) {
		fn=strdup(cfg->output_single);
	} else {
		return NULL;
	}
	if (!fn) {
		perror("malloc");
		exit(1);
	}
	return fn;
}

void record_output_file(const char *fname, const char *fname2, const char *message)
{
	int fd = open(fname2,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0666);
	if (fd < 0 ) {perror(fname2); return;}
	int mlen = strlen(message);
	int rc = write(fd, message, mlen);
	if (rc < 0 ) {perror("write"); return;}
	if (rc != mlen) {fprintf(stderr, "Aaugh!"); return;}
	rc = close(fd);
	if (rc < 0) {perror("close"); return;}
	rc = rename(fname2,fname);
	if (rc < 0) {perror("rename"); return;}
	return;
}

void advance_file_counter(struct roll_config *cfg, int corrupted)
{
	if (cfg->mode==1) {
		cfg->fcount++;
		if (cfg->fcount > 9999) {
			cfg->fcount=0;
			cfg->mode=0;
		}
	}
	if (cfg->mode==2 && !corrupted) cfg->mode=0;
}

void read_control_file(struct roll_config *cfg)
{
	char buff[200];
	FILE *f = fopen(cfg->command_file,"r");
	if (!f) return;
	while (fgets(buff,sizeof buff, f)) {
		jdrive_cmd(buff, cfg);
	}
	DPRINTF("mode=%d  excite_cycles=%d  excite_pattern=%d  local_stack=%d  output_filebase='%s'\n",
		cfg->mode, cfg->excite_cycles, cfg->excite_pattern, cfg->local_stack, cfg->output_filebase);
	fclose(f);
}

void change_files(struct roll_config *cfg, int corrupted)
{
	if (cfg->fhand) {
		fwrite("\n", 1, 1, cfg->fhand);  // keep Fortran happy?
		int rc = fclose(cfg->fhand);
		if (rc != 0) {perror("fclose");}
		if (corrupted) {
			unlink(cfg->meta_file);
		} else {
			record_output_file(cfg->meta_file, cfg->meta_file_new, cfg->actual_file);
		}
		DPRINTF("file %s complete, corrupted=%d\n", cfg->actual_file, corrupted);
		cfg->fhand = 0;
		if (cfg->actual_file) free(cfg->actual_file);
		cfg->actual_file = 0;
		advance_file_counter(cfg, corrupted);
	}
	read_control_file(cfg);
	if (corrupted) return;
	if (cfg->actual_file) free(cfg->actual_file);
	cfg->actual_file = output_file_gen(cfg);
	DPRINTF("actual_file = (%s)\n",cfg->actual_file);

	if (cfg->actual_file) {
		FILE *f = fopen(cfg->actual_file, "w");
		cfg->fhand = f;
		if (!f) {
			perror("fopen");
			return;
		}
		DPRINTF("writing file %s\n", cfg->actual_file);
		uxo_write_header(f, count_samples(cfg), cfg);
	}
}

#ifdef LIVE
void send_control(struct roll_config *cfg)
{
	usb_dev_handle *udh = cfg->udh;
	int v = cfg->extra_per<<24 | (cfg->excite_per-1)<<12 | cfg->adc_period;
	write_fpga_reg(udh, 70, 1);  // turn off data source
	write_fpga_reg(udh, 64, cfg->cnv_pos);
	write_fpga_reg(udh, 65, v);
	write_fpga_reg(udh, 66, cfg->gain_switch);
	write_fpga_reg(udh, 67, int(cfg->xmit_pulse*100-.5));
	write_fpga_reg(udh, 68, cfg->excite_pattern<<8 | cfg->excite_cycles);
	write_fpga_reg(udh, 71, cfg->force_low_gain);
	write_fpga_reg(udh, 72, cfg->odom_xor);
}

fusb_ephandle *setup_transmit(struct roll_config *cfg)
{
	usb_dev_handle *udh = cfg->udh;
	static const int FUSB_BLOCK_SIZE = 16 * (1L << 10);       // 16 KB
	static const int FUSB_BUFFER_SIZE = 8 * (1L << 20);       // 8 MB
	static const int FUSB_NBLOCKS    = FUSB_BUFFER_SIZE / FUSB_BLOCK_SIZE;

	fusb_devhandle *d_devhandle = fusb_sysconfig::make_devhandle (udh);
	if (d_devhandle == 0) {fprintf(stderr,"take_data abort 2\n");return 0;}
	fusb_ephandle *d_ephandle = d_devhandle->make_ephandle (6, true, FUSB_BLOCK_SIZE, FUSB_NBLOCKS);
	if (d_ephandle == 0) {fprintf(stderr,"take_data abort 3\n");return 0;}
	return d_ephandle;
}

void *engage_transmit(struct roll_config *cfg, fusb_ephandle *d_ephandle)
{
	usb_dev_handle *udh = cfg->udh;
	write_fpga_reg(udh, 4, 0x08);  // reset FPGA RX FIFO
	usrp_set_fpga_rx_reset(udh, true);   // reset FX2 FIFO
	usrp_set_fpga_rx_reset(udh, false);  // reset FX2 FIFO
	usrp_set_fpga_rx_enable(udh, true);
	write_fpga_reg(udh, 4, 0x02);  // FPGA FIFO operate
	usb_resetep(udh, 6);
	d_ephandle->start();
	write_fpga_reg(udh, 70, acq_mode(cfg));    // turn on data source
	// here it comes!
	return d_ephandle;
}

void stop_transmit(struct roll_config *cfg, fusb_ephandle *d_ephandle)
{
	usb_dev_handle *udh = cfg->udh;
	write_fpga_reg(udh, 70, 1);  // turn off data source
	d_ephandle->stop();	
}

void block_write(const char *data, size_t len, struct roll_config *cfg)
{
	if (cfg->fhand && cfg->mode!=0 && len>0) {
		size_t rc = fwrite(data, 1, len, cfg->fhand);
		if (rc != len) { perror("fwrite"); }
	}
}

void dump_frame(const unsigned short *mem, long int len)
{
	unsigned long int u;
	for (u=0; u<(unsigned int) len; u++) {
		printf("%4.4x ", mem[u]);
		if (u%16==15) putchar('\n');
	}
}

void set_with_wrap(int *hold, int hardware)
{
	/* assumes int is longer than 16 bits */
	int old_hardware = *hold & 0xffff;
	int delta = hardware - old_hardware;
	if (delta >  32768) delta -= 65536;
	if (delta < -32768) delta += 65536;
	*hold += delta;
}

void uxo_process_data(const char *data, long int len, struct roll_config *cfg)
{
	unsigned const short *mem = (unsigned const short *)data;
	unsigned int u;
	int start=-1;
	static int arm=0;
	static int count_started=0;
	static unsigned short count;
	static unsigned char odom_byte[8];
	static int odom_print=0;

	if (count_started == 0) {
		count=mem[12];
		count_started=1;
	}
	unsigned int sequence_errors=0;
	for (u=0; u<(unsigned int)len/32; u++) {
		unsigned c = mem[u*16+12];
		if (c != count) { 
			sequence_errors++;
		}
		count=c+1;
		if ((c&0x38) == 0x30) { /* odometer */
			unsigned oix = c&0x7;
			unsigned ob = mem[u*16+15] & 0xff;
			if (oix == 0) {
				odom_print = 1;
			}
			if (odom_print && odom_byte[oix] != ob) {
				odom_print = 2;
				odom_byte[oix] = ob;
			}
			if ((oix == 7) && odom_print==2) {
				set_with_wrap(cfg->odom_out+0, odom_byte[1]*256 + odom_byte[0]);
				set_with_wrap(cfg->odom_out+1, odom_byte[3]*256 + odom_byte[2]);
				set_with_wrap(cfg->odom_out+2, odom_byte[5]*256 + odom_byte[4]);
				set_with_wrap(cfg->odom_out+3, odom_byte[7]*256 + odom_byte[6]);
#if 0
				cfg->odom_out[0] = odom_byte[1]*256 + odom_byte[0];
				cfg->odom_out[1] = odom_byte[3]*256 + odom_byte[2];
				cfg->odom_out[2] = odom_byte[5]*256 + odom_byte[4];
				cfg->odom_out[3] = odom_byte[7]*256 + odom_byte[6];
#endif
				cfg->odom_out[4] = mem[u*16+15]>>8;
				if (cfg->odom_print) {
					printf("odometer: %5d %5d %5d %5d  ",
					cfg->odom_out[0],
					cfg->odom_out[1],
					-cfg->odom_out[2],
					cfg->odom_out[3]);
					unsigned raw = cfg->odom_out[4] ^ cfg->odom_xor;
					for (unsigned uu=0; uu<6; uu++) {
						if (uu==3) putchar(' ');
						putchar(raw&0x20?'-':'*');
						raw = 2*raw;
					}
					putchar('\n');
				/* for (unsigned uu=0; uu<8; uu++) printf(" %2.2x", odom_byte[uu]); */
				/* printf("\n"); */
					fflush(stdout);
				}
				odom_print = 0;
			}
		} else {
			odom_print = 0;
		}
		/* upper nibble of mem[u*16+13]:
		 * 02, 01  -> excite X
		 * 08, 04  -> excite Y
		 * 20, 10  -> excite Z
		 * 42     -> trigger?
		 * XXX For now, assume mode 5, wait for non-X followed by X+
		 */
		if (1) {
			unsigned short arm_masks[]={0xbf00,0x0100,0x0400,0x1000,0,0x3c00};
			unsigned short go_patts []={0x0000,0x0200,0x0800,0x2000,0,0x0200};
			unsigned short bits=mem[u*16+13];
			unsigned short arm_mask=arm_masks[cfg->excite_pattern];
			unsigned short go_patt =go_patts[cfg->excite_pattern];
			if (arm && (bits & 0xbf00) == go_patt) start=u*32;  /* armed X+ */
			arm =      (bits & arm_mask) != 0;  /* non-X */
			
		} else {
			start=0;
		}
	}
	if (cfg->sadc_print && !sequence_errors) {
		unsigned short s, d;
		int chan=7, val;
		int started=0;
		static int sadc_count=0;
		
		if (sadc_count%(cfg->local_stack ? 10 : 100) == 0) {
			printf("%6d:", sadc_count);
			for (unsigned uu=0; uu<64; uu++) {
				s=mem[uu*16+12];
				d=mem[uu*16+14];
				if (s%4==3) {
					chan=d>>12;
					val=d&0xfff;
					if (chan==0) started=1;
					if (started) printf(" %5d",val);
					if (chan==7 && started) {printf("\n"); fflush(stdout); break;}
				}
			}
		}
		sadc_count++;
	}
	if (sequence_errors) {
		fprintf(stderr,"%u sequence errors, file %s\n",sequence_errors,cfg->actual_file);
		change_files(cfg,1);
		cfg->cycles_handled=0;
		return;
#if 0
		dump_frame(mem, len);
		exit(3);
#endif
	}
	if (start!=-1) {
		int net_excite_cycles = cfg->excite_cycles;
		if (cfg->excite_pattern != 5) net_excite_cycles *= 27;
		cfg->cycles_handled++;
		if (cfg->mode!=0 && cfg->cycles_handled < net_excite_cycles) {
			start=-1;
		}
	}
	if (start!=-1) {
		block_write(data, start, cfg);
		change_files(cfg,0);
		cfg->cycles_handled=0;
		block_write(data+start, len-start, cfg);
	} else {
		block_write(data, len, cfg);
	}
}

#endif

void simulate_acquisition(struct roll_config *cfg)
{
	/* user will get unexpected results for acquisition times > 1159 s */
	int n = cfg->adc_period * cfg->excite_per * cfg->excite_cycles;
	if (n<370400) n=370400;
	n=n*5;  /* XXX hack to slow things down */
	DPRINTF("delay %d\n",n);
	struct timespec acq_delay;
	acq_delay.tv_sec  =        n / 1851851 ;
	acq_delay.tv_nsec = 540 * (n % 1851851);
	nanosleep(&acq_delay,NULL);

	if (cfg->mode != 0 && cfg->fhand) {
		size_t byte_length;
		short *data = fake_data(cfg, &byte_length);
		fwrite(data, 1, byte_length, cfg->fhand);
	}
	change_files(cfg,0);
}

char *pid_append(const char *s)
{
	char *t = (char *) malloc(strlen(s)+16);
	if (!t) return t;
	sprintf(t,"%s.%d", s, getpid());
	return t;
}

static char *file_mmap(const char *fname)
{
	int fd;
	char *data;
	fd = open(fname, O_RDONLY);
	if (fd < 0) { perror(fname); return NULL; }
	data = (char *) mmap(0, 4096, PROT_READ, MAP_SHARED, fd, 0);
	if (data == NULL) { perror(fname); return NULL; }
	close(fd);
	return data;
}

int main(int argc, char *argv[])
{
	struct roll_config cfg={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,{0,0,0,0,0},0,0,0,0,0,0,0,0,0,0,0
#ifdef LIVE
	,0
#endif
	};
	/* default configuration */
	cfg.excite_cycles=2;
	cfg.local_stack=1;   /* start in low-bandwidth mode */
	cfg.adc_period=400;
	cfg.excite_per=463;
	cfg.excite_pattern=5;
	cfg.test_stack=0;
	cfg.adc_period=400;
	cfg.excite_per=463;
	cfg.extra_per=0;
	cfg.gain_switch=0;
	cfg.cnv_pos=98;
	cfg.xmit_pulse=40;
	cfg.force_low_gain=0x30;
	cfg.odom_xor=18;
	cfg.command_file="control.in";
	cfg.meta_file="temp";
	cfg.meta_file_new=pid_append(cfg.meta_file);
	DPRINTF("meta_file_new = (%s)\n", cfg.meta_file_new);
	unlink(cfg.meta_file);  /* no data yet */

	char *gname = getenv("GPS_RING_FILE");
	if (gname) cfg.gps_ring = file_mmap(gname);

	if (argc>1) {
		if (strcmp(argv[1],"ver")==0) {
			fprintf(stderr, "jdrive version " JDRIVE_VERSION "\n");
		} else if (strcmp(argv[1],"sim")==0) {
			read_control_file(&cfg);
			for (;;) {
				simulate_acquisition(&cfg);
			}
		} else if (strcmp(argv[1],"odom")==0) {
			cfg.odom_print=1;
		} else if (strcmp(argv[1],"sadc")==0) {
			cfg.sadc_print=1;
		} else {
			fprintf(stderr, "usage: %s [ver|sim|odom|sadc]\n",argv[0]);
			return 1;
		}
	}
	long int to_read = 16*512;  /* 512 is USB packet size, one excitation pulse is about 29 packets */
	char *dest = (char *) malloc(to_read);
	if (!dest) {perror("malloc"); exit(1);}
	struct usb_dev_handle *udh = setup_uxo(0);   // XXX hard-coded board number 0
	if (!udh) exit(1);
	cfg.udh = udh;
	read_control_file(&cfg);
	cfg.param_changed=0;
	send_control(&cfg);
	fusb_ephandle *d_ephandle = setup_transmit(&cfg);
	engage_transmit(&cfg, d_ephandle);
	for (;;) {
		/* program should spend most of its clock time in
		 * following read() call */
		long int ret=d_ephandle->read (dest, to_read);
		if (ret != to_read) {
			fprintf(stderr,"aaugh!\n");
		} else {
			uxo_process_data(dest, ret, &cfg);
		}
		if (cfg.param_changed) {
			stop_transmit(&cfg, d_ephandle);
			send_control(&cfg);
			engage_transmit(&cfg, d_ephandle);
			cfg.param_changed=0;
		}
	}
}
