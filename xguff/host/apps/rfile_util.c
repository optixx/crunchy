#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include "rfile_util.h"

/* Global data filled in when the input file is processed
 */
static const char *rdrive_version   = NULL;
static const char *doc              = NULL;
static const char *utc_date_stamp   = NULL;
static unsigned int utc_unix        = 0;
static unsigned int utc_unix_ms     = 0;
static unsigned int frames          = 0;

/* pointer to the beginning of FPGA data */
static unsigned char *fpga_start;

#define ENTRY(x) {#x, &x}
static struct {
	const char *name;
	const char **pointer;
} asettings[] = {
	ENTRY(rdrive_version),
	ENTRY(doc),
	ENTRY(utc_date_stamp)
};

static struct {
	const char *name;
	unsigned int *value;
} settings[] = {
	ENTRY(utc_unix),
	ENTRY(utc_unix_ms),
	ENTRY(frames)
};
#undef ENTRY

/* ============================================
 * Preamble: extremely fundamental C hacking
 * ============================================
 */
static __inline char *raw_utoa(unsigned int i, char *p)
{
	do {
		*--p = '0' + i % 10;
		i /= 10;
	} while (i > 0);
	return p;
}

/* Note: the only time raw_itoa can fail to get the right answer
 * is when i == INT_MIN, and then only on truly bizarre computers
 * (which the #if will flag at compile-time).  I brought this up
 * many times on Usenet groups comp.lang.c and comp.unix.programmer.
 * The latest and most definitive thread can be found at
 *    http://www.google.com/groups?th=e60f317e1b91603
 * Since performance is the goal here, I won't attempt any of the
 * workarounds suggested in that conversation.
 */
#if -(INT_MIN+1) > (UINT_MAX-1)
#error this computer may be conforming, but it is too strange for this program
#endif

static __inline char *raw_itoa(int i, char *p)
{
	unsigned int x = (i>=0) ? (unsigned int) i : -(unsigned int) i;
	p = raw_utoa(x,p);
	if (i<0) *--p = '-';
	return p;
}

static __inline int to_hex(int v)
{
	return (v<10) ? ('0'+v) : ('A'-10+v);
}

/* ============================================
 * Subroutines: specific to UXO data files
 * ============================================
 */
/* Nota bene: the following macro only works properly when
 * "data" is declared as unsigned char *.
 */
#define D(i) (data[2*(i)] | (data[2*(i)+1]<<8))

unsigned int check_sequence(unsigned char *data, unsigned int nrows)
{
	unsigned short seq, seq_good;
	unsigned int i, errors=0;
	seq_good = D(15) & 0xff;
	for (i=0; i<nrows; i++) {
		seq = D(i*16+15) & 0xff;
		if (0) printf("debug: %u %u %u\n", i, seq, seq_good);
		if (seq != seq_good) errors++;
		seq_good = (seq+1) & 0xff;
	}
	return errors;
}

static void parset(const char *keyword, size_t keylen, const char *value)
{
	unsigned int u;
	for (u=0; u<(sizeof(asettings)/sizeof(asettings[0])); u++) {
		if (strncasecmp(keyword, asettings[u].name, keylen)==0) {
			*asettings[u].pointer = value;
			return;
		}
	}

	for (u = 0; u<(sizeof(settings)/sizeof(settings[0])); u++) {
		if (strncasecmp(keyword, settings[u].name, keylen)==0) {
			/* broken fprintf(stderr, "hit %s\n", keyword); */
			*settings[u].value = strtoul(value, NULL, 0);
			return;
		}
	}
	fputs("unknown keyword (", stderr);
	for (u = 0; u<keylen; u++) fputc(keyword[u], stderr);
	fputs(")\n", stderr);
}

static void process_line(char *buff)
{
	size_t keylen;
	char *p1=buff;
	if (!isprint(buff[0])) return;  /* must have been a blank line */
	for (;*p1;p1++) {
		if (*p1==':') {
			keylen = p1-buff;
			while (*(++p1)==' ') { /* empty */ }
			parset(buff, keylen, p1);
			return;
		}
	}
}

unsigned char *read_header(char *data)
{
	char *s=data;
	utc_date_stamp = NULL;
new_line:
	if (strncmp(s,"%%data%%",8)==0) {
		s=s+8;
		while (*s=='.') { s++; }
		if (*s!='\n') { return NULL; }
		s++;
		/* fprintf(stderr, "success! s=%p\n", s); */
		if (((long)s & 0x7) != 0) {
			fprintf(stderr, "alignment glitch: s=%p\n", s);
			return NULL;
		}
		return (unsigned char*) s;  /* success! */
	} else {
		char *l=s;
		while (isprint(*s)) { s++; }
		if (*s=='\n') { s++; process_line(l); goto new_line; }
	}
	fprintf(stderr, "no valid data header\n");
	return NULL;
}

void llrf_print_slow(void word_handler(unsigned v, FILE *f, unsigned *burst_age))
{
	unsigned u, v;
	unsigned char *data=fpga_start;  /* set up for D() */
	for (u=0; u<frames; u++) {
		v = D(u*16+15);
		word_handler(v,stdout,NULL);
	}
}

void print_set_map(unsigned char map[64])
{
	unsigned u, v;
	const char *rnames[]={
		"power", "conf", "set1", "set2",
	        "gain", "bode", "calz", "t0",
	        "t1", "t2", "t3", "util",
	        "pstep", "modu", "klys", ""};
	for (u=0; u<16; u++) {
		v=map[u+0] | map[u+16]<<8 | map[u+32]<<16 | map[u+48]<<24;
		printf(" %2d %5s 0x%8.8x\n", u, rnames[u], v);
	}
}

void llrf_print_sets(void)
{
	unsigned u, v;
	int arm=0;
	unsigned char map[64];
	unsigned char *data=fpga_start;  /* set up for D() */
	for (u=0; u<frames; u++) {
		v = D(u*16+15);
		if ((v&0xc0) == 0x80) {
			unsigned ix=v&0x3f;
			if (ix==0) arm=1;
			map[ix]=(v>>8U)&0xff;
			if (ix==63 && arm) {
				arm=0;
				print_set_map(map);
			}
		}
	}
}

void llrf_print_acoustic(void)
{
	unsigned u, v;
	int arm=0;
	unsigned char map[16];
	unsigned char *data=fpga_start;  /* set up for D() */
	for (u=0; u<frames; u++) {
		v = D(u*16+15);
		if ((v&0xf0) == 0xe0) {
			unsigned ix=v&0x0f;
			if (ix==0) arm=1;
			map[ix]=(v>>8U)&0xff;
			if (ix==15 && arm) {
				unsigned jx;
				arm=0;
				for (jx=0; jx<4; jx++) {
					printf("%d: 0x%x\n", jx,
						map[jx*4+0] <<  0 | map[jx*4+1] <<  8 | 
						map[jx*4+2] << 16 | map[jx*4+3] << 24 );
				}
			}
		}
	}
}

static double vscale(int decim)
{
	int b=decim-1;
	int c=1;
	while (b>7) {
		b=b/2;  c=c*4;
	}
	double s1=decim*decim/(double) c;
	double s2=1.0/(s1*8192.0);
	printf("a=%d  b=%d  c=%d  %.3f\n", decim, b, c, s1);
	return s2;
}

static unsigned int get_aux_byte(unsigned int offset)
{
	unsigned u, v;
	unsigned char *data=fpga_start;  /* set up for D() */
	u = D(15)&0xff;  /* first byte index */
	v = D((256-u+offset)*16+15);
	if ((v&0xff) != offset) fprintf(stderr,
		"oops, %d != %d\n", v&0xff, offset);
	return v>>8U;
}

static unsigned int get_decim(void)
{
	int d = get_aux_byte(0x80+32+1);
	int h = get_aux_byte(0x80+16+1);
	printf("get_decim %d 0x%x\n", d, h);
	if (!(h&0x01)) d=d*2;  // bypass_hfilt
	return d;
}

double llrf_vscale(void)
{
	return vscale(get_decim());
}

/* DSP frequency = nfreq * 48 MHz / 2^24 */
unsigned int get_nfreq(void)
{
	unsigned u, v=0;
	for (u=0; u<4; u++) {
		v = (v << 8U) | get_aux_byte(99-u);
	}
	return v;
}

double llrf_tscale(void)
{
	double dt = 16777216.0 / (48.0e6 * get_nfreq());  // seconds
	return(get_decim()*2*dt);
}

void llrf_print_fast(void)
{
	unsigned int u, j;
	unsigned char *data=fpga_start;  /* set up for D() */
	int v;
	char *p, buf[54];
	buf[53]='\0';
	buf[52]='\n';
	for (u=0; u<frames; u++) {
		for (j=u*16; j<u*16+15; j=j+5) {
			p=buf+52;
			v = (D(j+3)<<4) | ((D(j+4)&0xf000) >> 12);  if (v>524287) v-=1048576;  p = raw_itoa(v,p);  *--p = ' ';
			v = (D(j+2)<<4) | ((D(j+4)&0x0f00) >>  8);  if (v>524287) v-=1048576;  p = raw_itoa(v,p);  *--p = ' ';
			v = (D(j+1)<<4) | ((D(j+4)&0x00f0) >>  4);  if (v>524287) v-=1048576;  p = raw_itoa(v,p);  *--p = ' ';
			v = (D(j+0)<<4) | ((D(j+4)&0x000f)      );  if (v>524287) v-=1048576;  p = raw_itoa(v,p);
			/* printf("%d ", v); */ /* too slow */
			fputs(p,stdout);
		}
	}
}

/* returns number of points */
unsigned int llrf_fill_data(double **xdata_p)
{
	unsigned int u;
	unsigned int d;
	double *xdata;
	double vs=llrf_vscale()*100.0;
	unsigned int npt = frames*3;
	for (d=0; d<5; d++) {  // 4 data sets plus time coordinate
		xdata_p[d] =(double *) realloc(xdata_p[d],npt*sizeof *xdata);
		if (xdata_p[d] == NULL) {
			perror("realloc");
			return 0;
		}
	}
	unsigned char *data=fpga_start;  /* set up for D() */
	xdata=xdata_p[0];
	double dt=1.0/npt;
	for (u=0; u<npt; u++) {
		xdata[u]=dt*u;
	}
	xdata=xdata_p[1];
	for (u=0; u<npt; u++) {
		unsigned j=u*16/3;
		int v = (D(j+0)<<4) | ((D(j+4)&0x000f)      );  if (v>524287) v-=1048576;
		xdata[u]=vs*v;
	}
	xdata=xdata_p[2];
	for (u=0; u<npt; u++) {
		unsigned j=u*16/3;
		int v = (D(j+1)<<4) | ((D(j+4)&0x00f0) >>  4);  if (v>524287) v-=1048576;
		xdata[u]=vs*v;
	}
	xdata=xdata_p[3];
	for (u=0; u<npt; u++) {
		unsigned j=u*16/3;
		int v = (D(j+2)<<4) | ((D(j+4)&0x0f00) >>  8);  if (v>524287) v-=1048576;
		xdata[u]=vs*v;
	}
	xdata=xdata_p[4];
	for (u=0; u<npt; u++) {
		unsigned j=u*16/3;
		int v = (D(j+3)<<4) | ((D(j+4)&0xf000) >> 12);  if (v>524287) v-=1048576;
		xdata[u]=vs*v;
	}
	return npt;
}

void llrf_print_raw(void)
{
	unsigned int u, v;
	int j;
	unsigned char *data=fpga_start;  /* set up for D() */
	for (u=0; u<frames; u++) {
		for (j=0; j<16; j++) {
			v=D(u*16+j);
			putchar(to_hex( v>>12    ));
			putchar(to_hex((v>>8)&0xf));
			putchar(to_hex((v>>4)&0xf));
			putchar(to_hex( v    &0xf));
			putchar(' ');
		}
		putchar('\n');
	}
}

static void *file_map_start=NULL;
static size_t file_map_length=0;;

char *file_mmap(const char *fname, size_t *fsize)
{
	int fd;
	char *data;
	struct stat sbuf;
	if (stat(fname, &sbuf)<0) { perror("stat"); return NULL; }
	if (fsize) *fsize = sbuf.st_size;

	fd = open(fname, O_RDONLY);
	if (fd < 0) { perror(fname); return NULL; }

	data = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (data == NULL) { perror(fname); return NULL; }
	file_map_start = data;
	file_map_length = sbuf.st_size;
	close(fd);
	return data;
}

void llrf_unmap(void)
{
	int rc=0;
	if (file_map_start) munmap(file_map_start, file_map_length);
	if (rc != 0) {
		perror("munmap");
	}
}

int llrf_read_header(const char *fname, int verbose)
{
	char *data;
	size_t fsize, theory_size;
	int errors;
	data = file_mmap(fname, &fsize);
	if (data == NULL) return 1;
	fpga_start = read_header(data);
	if (fpga_start == NULL) return 2;
	theory_size = frames*32+(fpga_start-(unsigned char *)data)+1;
	if (theory_size != fsize) {
		fprintf(stderr,"file %s: size %zu mismatch with prediction (%zu)\n", fname, fsize, theory_size);
		return 2;
	}
	errors = check_sequence(fpga_start,frames);
	if (errors != 0) {
		fprintf(stderr,"file %s: %d sequence errors in %d samples\n", fname, errors, frames);
		return 2;
	}
	if (verbose) fprintf(stderr,"file %s: %d frames (%d samples)\n", fname, frames, frames*3);
	return 0;
}
