/* Flag string for output file: */
#define RDRIVE_VERSION "0.1"

/* Defects:
 *   randomly and rarely fails to start
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
#include <sys/ioctl.h>

#include "uxo_util.h"
#include "usrp_prims.h"

extern "C" {
void aux_check_word(unsigned int v, FILE *print, unsigned *burst_age);
}

#if 0
#define DPRINTF(format, args...)  fprintf(stderr, format, ##args)
#else
#define DPRINTF(format, args...)
#endif

int burst_mode=0;   // XXX global

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

/* stuff to put in header:
   rdrive_version: 0.1
   utc_time:
   utc_ms:
   frames:
   %%data%%...

   commands to process:
   p              pause
   r              run
   s              single
   x              exit
   b <text>       file base
   w <reg> <data> write fpga register
   n <num>        set number of frames

   send to control channel:
   g <filename>   garbage
   f <filename>   good file

 */

struct roll_config {

	int next_mode;  /* changeable by command */
	int mode;
	int command_fd;
	int status_fd;
	FILE *status_f;
	unsigned int frames;  /* changeable by command */
	unsigned int frames_handled;
	char *output_filebase;  /* changeable by command */
	int fcount;
	FILE *fhand;
	char *actual_file;

	usb_dev_handle *udh;
};

typedef void (*action_routine)(const char *, struct roll_config *);

static void do_p(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	DPRINTF("do_p (%s)\n", s);
	c->next_mode = 0;
}

static void do_r(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	DPRINTF("do_r (%s)\n", s);
	c->next_mode = 1;
}

static void do_s(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	DPRINTF("do_s (%s)\n", s);
	c->next_mode = 2;
}

static void do_x(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	(void) c; /* unused */
	DPRINTF("do_x (%s)\n", s);
	exit(0);
}

static void do_w(const char *s, struct roll_config *c)
{
	unsigned long reg, val;
	char *p1, *p2;
	DPRINTF("do_w (%s)\n", s);
	reg = strtoul(s, &p1, 0);
	val = strtoul(p1, &p2, 0);
	usb_dev_handle *udh = c->udh;
	if (p2 != s) {
		// XXX hack: snoop on config register for burst mode, so we can
		// select the right frame triggering logic
		if (0 && reg == 81) {
			burst_mode = (val >> 25U) & 1;
			fprintf(stderr, "burst_mode=%d\n", burst_mode);
		}
		write_fpga_reg(udh,reg,val);
	}
}

static void do_b(const char *s, struct roll_config *c)
{
	DPRINTF("do_b (%s)\n", s);
	char *b = get_param(s);
	if (!b) return;

	if (c->output_filebase==NULL || strcmp(b, c->output_filebase)!=0) {
		c->fcount = 1;
	}
	if (c->output_filebase) free(c->output_filebase);
	c->output_filebase = b;
}

static void do_n(const char *s, struct roll_config *c)
{
	DPRINTF("do_n (%s)\n", s);
	c->frames = strtoul(s, NULL, 0);
}

static void do_nothing(const char *s, struct roll_config *c)
{
	(void) s; /* unused */
	(void) c; /* unused */
	DPRINTF("do_nothing (%s)\n", s);
}

#define CMD(x) { #x, do_ ## x}
static struct {
	const char *command;
	action_routine action;
} cmd_table[] = {
	CMD(p),
	CMD(r),
	CMD(s),
	CMD(x),
	CMD(w),
	CMD(b),
	CMD(n),
	{"#", do_nothing}
};
#undef CMD

void rdrive_cmd(const char *cmd, struct roll_config *cfg)
{
	unsigned u;
	for (u=0; u<sizeof cmd_table/sizeof *cmd_table; u++) {
		const char *check = cmd_table[u].command;
		if (*cmd == *check) {
			const char *s=cmd+1;
			while (isspace(*s)) {s++;}
			cmd_table[u].action(s, cfg);
			/* printf("ready\n");  fflush(stdout); */
			return;
		}
	}
	DPRINTF("unhandled %s\n",cmd);
}

void input_avail(int fd, struct roll_config *cfg)
{
	char inbuf[1024];
	static char cmdbuf[102];
	static char *dst=cmdbuf;
	ssize_t n=read(fd,inbuf,1024);
	if (0) printf("read %zd bytes on control port\n", n);
	if (n==0) { /* end of file */
		/* Fl::remove_fd(0); */
	} else if (n<0) {
		perror("command input");
	} else for (unsigned u=0; u<(unsigned)n; u++) {
		if (inbuf[u] == '\n') {
			*dst=0;
			rdrive_cmd(cmdbuf, cfg);
			dst=cmdbuf;
		} else if (dst-cmdbuf < 100 && inbuf[u] >= 32 && inbuf[u]<127) {
			*dst++=inbuf[u];
		}
	}
}

void llrf_write_header(FILE *f, struct roll_config *cfg)
{
	/* gettimeofday() is not standard */
	/* clock_gettime needs USE_POSIX199309 and -lrt */
	struct timespec now;
	fprintf(f,"rdrive_version: " RDRIVE_VERSION "\n");
	fprintf(f,"doc: http://recycle.lbl.gov/llrf4/\n");
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
	fprintf(f,"frames: %d\n", cfg->frames);

	fputs("%%data%%",f);
	long p=ftell(f);
	fwrite(".........", 1, 8-(p+1)%8, f);   // pad to double-word boundary
	fputs("\n",f);
}

char *output_file_gen(struct roll_config *cfg)
{
	char *fn;
	if (cfg->mode != 0 && cfg->output_filebase) {
		fn=(char *)malloc(strlen(cfg->output_filebase)+10);
		if (fn) sprintf(fn,"%s_%4.4d.bin",cfg->output_filebase,cfg->fcount);
	} else {
		return NULL;
	}
	if (!fn) {
		perror("malloc");
		exit(1);
	}
	return fn;
}

void advance_file_counter(struct roll_config *cfg)
{
	if (cfg->mode!=0) {
		cfg->fcount++;
		if (cfg->fcount > 99) {
			cfg->fcount=0;
			// cfg->next_mode=0;
		}
	}
}

void change_files(struct roll_config *cfg, int corrupted)
{
	if (cfg->fhand) {
		fwrite("\n", 1, 1, cfg->fhand);  // keep Fortran happy?
		int rc = fclose(cfg->fhand);
		if (rc != 0) {perror("fclose");}
		DPRINTF("file %s complete, corrupted=%d\n", cfg->actual_file, corrupted);
		fprintf(cfg->status_f, "%c %s\n", corrupted ? 'g' : 'f', cfg->actual_file);
		fflush(cfg->status_f);
		cfg->fhand = 0;
		advance_file_counter(cfg);
	}
	if (cfg->actual_file) free(cfg->actual_file);
	cfg->actual_file = 0;

	cfg->mode = 0;
	if (corrupted) return;
	cfg->mode = cfg->next_mode;
	if (cfg->next_mode == 2) cfg->next_mode = 0;

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
		llrf_write_header(f, cfg);
	}
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
	if (0) printf("block write %d %d %zd\n", cfg->fhand!=NULL, cfg->mode, len);
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

void llrf_process_data(const char *data, long int len, struct roll_config *cfg)
{
	unsigned const short *mem = (unsigned const short *)data;
	unsigned int u;
	int start=-1;
	static int count_started=0;
	static unsigned short count;

	if (count_started == 0) {
		count=mem[15]&0xff;
		count_started=1;
	}
	unsigned int sequence_errors=0;
	unsigned int frames_now = (unsigned int)len/32;
	for (u=0; u<frames_now; u++) {
		unsigned c = mem[u*16+15]&0xff;
		if (0 && (c != count)) { 
			sequence_errors++;
		}
		count=(c+1)&0xff;
	}
	if (sequence_errors) {
		fprintf(stderr,"%u sequence errors, after %d frames handled, file %s\n", sequence_errors, cfg->frames_handled, cfg->actual_file);
		change_files(cfg,1);
		cfg->frames_handled=0;
		if (0) dump_frame(mem, len);
		if (0) exit(3);
		return;
	} else {
		unsigned burst_age=-1;
		for (u=0; u<frames_now; u++) {
			unsigned c = mem[u*16+15];
			aux_check_word(c, cfg->status_f, &burst_age);
			if (burst_mode && start==-1 &&
			    burst_age != -1U && burst_age<u) { // XXX not debugged
				fprintf(stderr,"u=%d burst_age=%d frames=%d\n", u, burst_age, cfg->frames_handled);
				if (1) start = (u - burst_age)*32;
			}
		}
	}
	if (cfg->frames_handled == 0) {
		if (~burst_mode) start=0;  /* temporary, always start */
	}
	if (cfg->frames_handled + frames_now > cfg->frames) {
		start = 32 * (cfg->frames - cfg->frames_handled);
		if (start > len) start=len;
		if (start < 0) start=0;
		/* these last two possibilities leave a file with a length
		 * that doesn't match the header.  It's not listed as
		 * corrupted, otherwise the next file won't start correctly. */
	}
	if (0) printf("%d %d %d %d\n", cfg->frames_handled, start, cfg->frames, cfg->mode);
	if (start!=-1 || cfg->mode == 0) {
		block_write(data, start, cfg);
		change_files(cfg,0);
		block_write(data+start, len-start, cfg);
		if (cfg->mode == 0)
			cfg->frames_handled = 0;
		else
			cfg->frames_handled = (len-start)/32;
	} else {
		block_write(data, len, cfg);
		cfg->frames_handled += frames_now;
	}
}

void read_control(struct roll_config *cfg)
{
	int flag;
	while (ioctl(cfg->command_fd, FIONREAD, &flag),flag>0) input_avail(cfg->command_fd, cfg);
}

int main(int argc, char *argv[])
{
	int which_board=0;
	struct roll_config cfg={0,0,0,0,0,0,0,0,0,0,0,0};

	cfg.frames=2048;   /* default */
	if (argc>1 && strcmp(argv[1],"ver")==0) {
		fprintf(stderr, "rdrive version " RDRIVE_VERSION "\n");
		return 0;
	}
	if (argc>2 && strcmp(argv[1],"-w")==0) {
		which_board = atoi(argv[2]);
		argc=argc-2;
		argv=argv+2;
	}
	fprintf(stderr,"using board %d\n",which_board);

	/* Like octplot, we can be called with a numeric file descriptor as argument,
	 * which was generated from a call to socketpair(2).
	 * Otherwise default to listening on stdin, and sending status on stdout.
	 */
	if (argc>1) {
		cfg.command_fd = atoi(argv[1]);
		fprintf(stderr,"using fd %d\n", cfg.command_fd);
		cfg.status_fd = cfg.command_fd;
	} else {
		cfg.command_fd = 0;  /* stdin */
		cfg.status_fd = 1;  /* stdout */
	}
	cfg.status_f = fdopen(cfg.status_fd, "w");
	if (cfg.status_f == NULL) {
		perror("fdopen");
		return 1;
	}

	long int to_read = 16*512;  /* 512 is USB packet size */
	char *dest = (char *) malloc(to_read);
	if (!dest) {perror("malloc"); exit(1);}
	struct usb_dev_handle *udh = setup_uxo(which_board);
	if (!udh) exit(1);
	cfg.udh = udh;
	read_control(&cfg);
	/* send_control(&cfg); */
	fusb_ephandle *d_ephandle = setup_transmit(&cfg);
	engage_transmit(&cfg, d_ephandle);
	for (;;) {
		/* program should spend most of its clock time in
		 * following read() call */
		long int ret=d_ephandle->read (dest, to_read);
		if (ret != to_read) {
			fprintf(stderr,"aaugh!\n");
		} else {
			llrf_process_data(dest, ret, &cfg);
		}
		read_control(&cfg);
	}
}
