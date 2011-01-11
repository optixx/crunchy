/* to do:
 *   refactor for more tables and less repetitious code
 *   scale factors and units on amplitudes?
 *   initial write_fpga_word()s
 *   fancy bode sketch?
 *   initialize ad9512 and write_fpga_reg 68 0x200   # trace_stream?
 *   acoustic program download?
 */

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Roller.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
// #include <GL/glut.h>
#define UCODE1_SINE

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>

#include "CubeViewUI.h"

FILE *out_f;  /* global */

struct fpga_word {
	int regno;
	int acou_r;
	uint32_t value;
};

struct fpga_word
	power_conf={80,0,0},
	conf={81,0,0x211a0032},  /* free_run, cw_mode, decim=26, D, C */
	set1={82,0,0},
	set2={83,0,0},
	gain={84,0,0},
	bode={85,0,0},
	calz={86,0,0},
	t0={87,0,0},
	t1={88,0,0},
	t2={89,0,0},
	t3={90,0,0},
	util={91,0,0},
	// pstep={92,0,0xb13b13b1},   direct access
	// modul={93,0,1},
	klys={94,0,0};

struct config_bit {
	char *label;
	struct fpga_word *word;
	int bit;
};

struct config_subword {
	char *label;
	struct fpga_word *word;
	int shift;
	int width;
	int mod1;
	int mod2;
	int wrange;
	double min;
	double max;
};

struct config_cpxword {
	char *label;
	struct fpga_word *word;
	int encoding;       // in FPGA word:  1 for R/phi, 2 for I/Q
	double mag;         // 0 to 1
	double phase;       // degrees
	Fl_Output *flout;   // to show phase numerically
};

struct dds_word {
	Fl_Output *flout;   // to show frequency shift numerically
};

struct menu_sel {
	char *name;
	int val;
	struct config_subword *w;
};

void write_fpga_word(int regno, uint32_t val)
{
	fprintf(out_f, "w %d 0x%x\n", regno, val);
	fflush(out_f);
}

static int acou_sel=0;
void write_fpga_word(struct fpga_word *f, uint32_t val)
{
	if (!f) return;
	if (f->acou_r && f->acou_r !=acou_sel) {
		write_fpga_word(78,f->acou_r);
		acou_sel=f->acou_r;
	}
	write_fpga_word(f->regno, val);
	f->value = val;
}

Fl_Output *current_dfile;
CubeViewUI *cv;
void process_file(char *file)
{
	current_dfile->value(file);
	cv->cube->set_file(file);
}

Fl_Output *sadc_out[9];
Fl_Output *other_out[4];
Fl_Output *acoustic_out[4];
void process_command(char *cmd)
{
	if (0) fprintf(stderr, "command (%s)\n", cmd);
	if (cmd[0] == 'a' && cmd[1] >= '0' && cmd[1] < '9') {
		unsigned u = cmd[1]-'0';
		char *p = cmd+2;
		while (*p==' ') p++;
		sadc_out[u]->value(p);
	} else if (cmd[0] == 's' && cmd[1] >= '0' && cmd[1] < '4') {
		unsigned u = cmd[1]-'0';
		char *p = cmd+2;
		while (*p==' ') p++;
		other_out[u]->value(p);
	} else if (cmd[0] == 'A' && cmd[1] >= '0' && cmd[1] < '4') {
		unsigned u = cmd[1]-'0';
		char *p = cmd+2;
		while (*p==' ') p++;
		acoustic_out[u]->value(p);
	} else if (cmd[0] == 'f') {
		char *p = cmd+1;
		while (*p==' ') p++;
		process_file(p);
	}
}

void stdin_callback(int fd, void *data)
{
	(void) data;   /* not used */
	char inbuf[1024];
	static char cmdbuf[102];
	static char *dst=cmdbuf;
	ssize_t n=read(fd,inbuf,1024);
	if (0) fprintf(stderr, "read %zd bytes on control port\n", n);
	if (n==0) { /* end of file */
		Fl::remove_fd(0);
	} else if (n<0) {
		perror("command input");
	} else for (unsigned u=0; u<(unsigned)n; u++) {
		if (inbuf[u] == '\n') {
			*dst=0;
			process_command(cmdbuf);
			dst=cmdbuf;
		} else if (dst-cmdbuf < 100 && inbuf[u] >= 32 && inbuf[u]<127) {
			*dst++=inbuf[u];
		}
	}
}

void simple_command(Fl_Widget *w, void *data)
{
	(void) w;  /* unused */
	char *d = (char *)data;
	fputs(d, out_f);
	fputc('\n', out_f);
	fflush(out_f);
}

char *fbase_peek=NULL;  // gross and disgusting
void fread_command(Fl_Widget *w, void *data)
{
	(void) w;  /* unused */
	(void) data; /* unused */
	if (fbase_peek) cv->cube->set_file(fbase_peek);
}

void quit_command(Fl_Widget *w, void *data)
{
	simple_command(w, data);
	exit(0);
}

void string_command(Fl_Widget *w, void *data)
{
	char *d = (char *)data;
	Fl_Input *ww = (Fl_Input *)w;
	if (d[0]=='b') {
		if (fbase_peek) free(fbase_peek);
		fbase_peek = strdup(ww->value());
	}
	fprintf(out_f, "%s %s\n", d, ww->value());
	fflush(out_f);
}

void config_bit_callback(Fl_Widget *w, void *data)
{
	struct config_bit *d = (struct config_bit *) data;
	Fl_Button *ww = (Fl_Check_Button *)w;
	int val = ww->value();
	int shift = d->bit;
	if (0) fprintf(stderr,"config_bit_callback (%s) %d %d\n", d->label, shift, val);
	uint32_t word = d->word->value;
	uint32_t old = word;
	word &= ~(1U<<shift);
	if (val) word |= (1U<<shift);
	if (0) fprintf(stderr,"old=%x new=%x\n", old, word);
	write_fpga_word(d->word, word);
}

void subword_set(struct config_subword *d, uint32_t uv)
{
	uint32_t word = d->word->value;
	uint32_t mask;
	if (d->width == 32) mask = ~0U;
	else mask = (1U << d->width) - 1;
	word = (word & ~(mask<<d->shift)) | ((uv & mask)<<d->shift);
	if (0) fprintf(stderr, "uv=%x mask=%x word=%x\n", uv, mask, word);
	write_fpga_word(d->word, word);
#ifndef UCODE1_SINE
	if (d->word->regno==77 && d->word->acou_r==71) { // ak1b XXX total hack
		write_fpga_word(&ak1, ~word);  // ak1
	}
#endif
}

void config_subword_callback(Fl_Widget *w, void *data)
{
	struct config_subword *d = (struct config_subword *) data;
	Fl_Valuator *ww = (Fl_Valuator *)w;
	double v = ww->value();
	if (0) fprintf(stderr, "config_subword_callback (%s) %f\n", d->label, v);
	double m = d->wrange;
	if (m==0) {
		m=pow(2,d->width)-1;
	}
	double s = m/ww->maximum();
	uint32_t uv = (uint32_t) (v*s);
	if (d->mod1) uv = (uv/d->mod1)*d->mod1 + d->mod2;
	subword_set(d, uv);
}

void menu_sel_callback(Fl_Widget *w, void *data)
{
	(void) w;  /* not used */
	struct menu_sel *d = (struct menu_sel *) data;
	subword_set(d->w, d->val);
}

void write_cpxword(struct config_cpxword *d)
{
	if (0) fprintf(stderr,"write_cpxword(%d, %f, %f)\n",
		d->encoding, d->mag, d->phase);
	uint32_t word;
	if (d->encoding == 1) {
		uint32_t p = (uint32_t) floor(d->phase*8192/45 + 0.5);
		// 39792=65528/1.64676, should match klys_amp in updown_tb.v
		uint32_t m = (uint32_t) floor(d->mag*39792 + 0.5);
		// set1 and set2 in set_engine.v use {m,p}
		word = (m&0xffff)<<16 | (p&0xffff);
	} else if (d->encoding == 2) {
		// gain and cal use I/Q encoding
		double rad = d->phase*3.14159265/180;
		uint32_t i = (uint32_t) floor(d->mag * sin(rad) * 32767 + 0.5);
		uint32_t q = (uint32_t) floor(d->mag * cos(rad) * 32767 + 0.5);
		word = (i&0xffff)<<16 | (q&0xffff);
	} else {
		fprintf(stderr,"misconfigured config_cpxword\n");
		word = 0;
	}
	write_fpga_word(d->word, word);
}

void mag_callback(Fl_Widget *w, void *data)
{
	Fl_Valuator *ww= (Fl_Valuator *)w;
	double v = ww->value();
	if (0) fprintf(stderr,"mag_callback %f\n", v);
	struct config_cpxword *d = (struct config_cpxword *) data;
	d->mag = v/ww->maximum();
	write_cpxword(d);
}

void phase_callback(Fl_Widget *w, void *data)
{
	Fl_Valuator *ww= (Fl_Valuator *)w;
	double v = ww->value();
	if (0) fprintf(stderr,"phase_callback %f\n", v);
	while (v > 180.0) {
		v = v-360.0;
		ww->value(v);
		if (0) fprintf(stderr, "wraparound + %f\n", v);
	}
	while (v < -180.0) {
		v = v+360.0;
		ww->value(v);
		if (0) fprintf(stderr, "wraparound - %f\n", v);
	}
	struct config_cpxword *d = (struct config_cpxword *) data;
	char ss[20];
	snprintf(ss,20,"%6.1f",v);
	d->flout->value(ss);
	d->phase = v;
	write_cpxword(d);
	// write_fpga_word(d->word, word);
}

void pstep_callback(Fl_Widget *w, void *data)
{
	Fl_Valuator *ww= (Fl_Valuator *)w;
	double v = ww->value();
	struct dds_word *d = (struct dds_word *) data;

	/* hard-code coarse ratio of 3/13 */
	unsigned long qq=1; /* was 5 */
	unsigned long x = 1024UL*(1024UL*3+759UL)*qq+(long)v*10;
	unsigned long pstep_coarse = x/(13*qq);
	unsigned long pstep_fine = (x%(13*qq))*63;
	double assumed_clock = 50.0;
	double real_hz = assumed_clock * x / (13*qq*1024*1024);
	uint32_t word = pstep_coarse << 12 | pstep_fine;
	if (1) printf("offset=%f  x=%lu  coarse=%lu  fine=%lu  config=0x%x offset=%.2f Hz\n",
		v, x, pstep_coarse, pstep_fine, word, real_hz);

	char ss[20];
	snprintf(ss,20,"%9.5f",real_hz);
	d->flout->value(ss);
	write_fpga_word(92,word);
}

void rdrive_connect(char *prog)
{
  int in_fd, out_fd;
  if (prog == NULL || *prog == 0) {
	in_fd = 0;
	out_fd = 1;
	if (0) fcntl(0, F_SETFL, O_NONBLOCK);   /* set stdin non-blocking */
  } else {
	int sv[2];   /* the pair of socket descriptors */
	char buffer[32];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
		perror("socketpair");
		exit(1);
	}
	sprintf(buffer,"%i",sv[1]);
	if (!fork()) {  // Child
		signal(SIGINT, SIG_IGN);
		execlp(prog, prog, buffer, NULL);
		perror(prog);
		_exit(1);
	}
	// Parent
	in_fd = sv[0];
	out_fd = sv[0];
  }
  if (0) fprintf(stderr, "fds %d %d\n", in_fd, out_fd);
  Fl::add_fd(in_fd, FL_READ, stdin_callback, 0);
  out_f = fdopen(out_fd, "w");
  if (out_f == NULL) {
	perror("fdopen");
	exit(1);
  }
}

int main(int argc, char **argv) {
  char *rdrive_prog = getenv("RDRIVE_PROG");
  if (!rdrive_prog) rdrive_prog="rdrive";
  rdrive_connect(rdrive_prog);
  // glutInit(&argc, argv);
  // controls alone fit in 340 x 600
  Fl_Window *window = new Fl_Window(940,620);
  Fl_Box *box = new Fl_Box(10,10,200,30,"rgui 0.4");
  box->box(FL_UP_BOX);
  box->labelsize(20);
  box->labelfont(FL_BOLD);

  struct config_bit power_bits[] = {
	{"ADC1",   &power_conf, 0},
	{"ADC2",   &power_conf, 1},
	{"ADC3",   &power_conf, 2},
	{"ADC4",   &power_conf, 3},
	{"DAC",    &power_conf, 4},
	{"Dallas", &power_conf, 5},
	{"MCP",    &power_conf, 6},
	{"PIN",    &power_conf, 7},
	{"sync",   &power_conf, 8},
	{"dsp",    &power_conf, 9},
	{"autopz", &conf, 31},
	{"fall_t", &conf, 30},
	{"free_r", &conf, 29},
	{"ext_t",  &conf, 28},
	// {"cw_cal", &conf, 27},
	// {"cw_set", &conf, 26},
	{"dbl_a",  &util,  6},
	{"dbl_b",  &util,  7},
	// {"acq",    &conf, 24},
	// {"ac_run", &util,  0},
	// {"ac_set", &util,  1},
	// {"open",   &util,  2},
	{"dbg0",   &util,  3},
	{"dbg1",   &util,  4},
	{"int_en", &util,  5},
	{"burst",  &conf, 25},
	{"hb_byp", &conf,  8},  // bypass_hfilt
	{"raw_d",  &conf,  9}}; // trace_raw_data

  Fl_Check_Button *cb[22];
  for (unsigned u=0; u<22; u++) { // findme 275 220
	int xp, yp;
	if (u<21) { xp = 275; yp = 195+u*17; }
	     else { xp =  80; yp = 149+u*17; }
	cb[u] = new Fl_Check_Button(xp,yp,55,23,power_bits[u].label);
	cb[u]->type(FL_TOGGLE_BUTTON);
	cb[u]->labelsize(12);
	cb[u]->callback(config_bit_callback,&(power_bits[u]));
	int defv = power_bits[u].word->value;
	int defb = (defv >> power_bits[u].bit) & 1;
	if (0) printf("cb[%d] default processing: %x %d\n", u, defv, defb);
	cb[u]->value(defb);
  }

#if 0
  struct config_subword other_subword[] = {
	//BW",   &bode,   16, 15,  0,  0, 0,  0.0, 1.0},
	{"int",  &bode,    0, 12,  0,  0, 0,  0.0, 1.0},
	{"t0",   &t0,      0, 18, 22, 18, 0,  0.0, 1.0},
	{"t1",   &t1,      0, 18,  0,  0, 0,  0.0, 1.0}, // XXX should be 24 (4 places)
	//t2",   &t2,      0, 18,  0,  0, 0,  0.0, 1.0},
	//t3",   &t3,      0, 18,  0,  0, 0,  0.0, 1.0},
	// {"pcoarse", &pstep, 12, 20, 0, 0},
	// {"pfine",   &pstep,  0, 12, 0, 0}
	// {"sync", &power_conf, 12, 4, 0, 0}
	// {"acou", &acou_d, 16, 16, 0, 0},
  };

  Fl_Value_Slider *sl[7];
  for (unsigned u=0; u<7; u++) {
	sl[u] = new Fl_Value_Slider(40,200+u*30,141,20,other_subword[u].label);
	sl[u]->type(FL_HOR_FILL_SLIDER);
	sl[u]->align(FL_ALIGN_LEFT);
	sl[u]->callback(config_subword_callback,&(other_subword[u]));
	sl[u]->range(other_subword[u].min,other_subword[u].max);
  }
#endif

  struct config_cpxword vectors[] = {
	{"seta", &set1, 1, 0, 0, NULL},
	{"setb", &set2, 1, 0, 0, NULL}
  };
  Fl_Value_Slider *v_sl[2];
  Fl_Roller *v_ph[2];
  Fl_Output *v_ou[2];
  for (unsigned u=0; u<2; u++) {
	v_sl[u] = new Fl_Value_Slider(40,80+u*30,140,20,vectors[u].label);
	v_sl[u]->type(FL_HOR_FILL_SLIDER);
	v_sl[u]->align(FL_ALIGN_LEFT);
	v_sl[u]->callback(mag_callback,&(vectors[u]));
	v_ou[u] = new Fl_Output(295,80+u*30,50,20,NULL);
	v_ou[u]->textsize(12);
	vectors[u].flout = v_ou[u];
	v_ph[u] = new Fl_Roller(190,80+u*30,100,20,NULL);
	v_ph[u]->type(FL_HORIZONTAL);
	v_ph[u]->range(-720,720.0);
	v_ph[u]->step(0.3);
	v_ph[u]->value(0.0);
	v_ph[u]->callback(phase_callback,&vectors[u]);
	phase_callback(v_ph[u],&vectors[u]);  /* initialize display & FPGA */
  }

  struct dds_word pstep_x={NULL};
  Fl_Roller *pstep_dial = new Fl_Roller(5,410,130,20,NULL);
  pstep_dial->type(FL_HORIZONTAL);
  pstep_dial->step(1);
  pstep_dial->value(0.0);
  pstep_dial->range(-1000,1000);
  Fl_Output *pstep_show = new Fl_Output(140,410,80,20,NULL);
  pstep_show->textsize(12);
  pstep_x.flout = pstep_show;
  pstep_dial->callback(pstep_callback,&pstep_x);

#if 0
  Fl_Value_Input *acou_s = new Fl_Value_Input(40,410,70,20,"acou");
  acou_s->when(FL_WHEN_ENTER_KEY);
  acou_s->align(FL_ALIGN_LEFT);
  acou_s->textsize(10);
  struct config_subword acou_ds= {"acou", &acou_d, 16, 16, 0, 0};
  acou_s->callback(config_subword_callback,&acou_ds);
  acou_s->value(0);
  acou_s->range(0,65535);
#endif

  struct config_subword decim={"decim", &conf, 16, 7, 0, 0, 0, 0.0, 127.0};
  Fl_Value_Slider *d_sl = new Fl_Value_Slider(60,570,170,20,decim.label);
  d_sl->type(FL_HOR_FILL_SLIDER);
  d_sl->align(FL_ALIGN_LEFT);
  d_sl->callback(config_subword_callback,&decim);
  d_sl->range(0,127);
  d_sl->step(1);
  d_sl->value(52);

  Fl_Input *dlen = new Fl_Input(280,570,50,20,"#pkts");
  dlen->when(FL_WHEN_ENTER_KEY);
  dlen->align(FL_ALIGN_LEFT);
  char *dlen_cmd = "n";
  dlen->callback(string_command,dlen_cmd);
  dlen->value("2048");

  Fl_Input *fbase = new Fl_Input(200,600,130,20,"fbase");
  fbase->when(FL_WHEN_ENTER_KEY);
  fbase->align(FL_ALIGN_LEFT);
  char *fbase_cmd = "b";
  fbase->callback(string_command,fbase_cmd);

  Fl_Button *run    = new Fl_Button(200,490,50,20,"Run");
  char *run_cmd = "r";
  run->callback(simple_command,run_cmd);

  Fl_Button *single = new Fl_Button(200,470,50,20,"Single");
  char *single_cmd = "s";
  single->callback(simple_command,single_cmd);

  Fl_Button *pause  = new Fl_Button(200,450,50,20,"Pause");
  char *pause_cmd = "p";
  pause->callback(simple_command,pause_cmd);

  Fl_Button *fread  = new Fl_Button(200,430,50,20,"Read");
  fread->callback(fread_command,NULL);

  Fl_Button *quit = new Fl_Button(280,10,50,20,"Quit");
  char *quit_cmd = "x";
  quit->callback(quit_command,quit_cmd);

  /* Channel 1 source select */
  Fl_Choice *ch1_sel = new Fl_Choice(210,530,60,30,"ch1");
  ch1_sel->align(FL_ALIGN_TOP);
  struct config_subword ch1_subword={"ch1", &conf, 0, 4, 0, 0, 0, 0.0, 1.0};
  struct menu_sel ch1_choices[] = {
	{"a",   0, &ch1_subword},
	{"b",   1, &ch1_subword},
	{"c",   2, &ch1_subword},
	{"d",   3, &ch1_subword},
	{"err", 4, &ch1_subword},
	{"fb",  5, &ch1_subword},
	{"cal", 6, &ch1_subword},
	{"pz",  7, &ch1_subword}};
  for (unsigned u=0; u<8; u++) {
	ch1_sel->add(ch1_choices[u].name, 0, menu_sel_callback, &(ch1_choices[u]), 0);
  }
  ch1_sel->value(2);  /* default */

  /* Channel 2 source select */
  Fl_Choice *ch2_sel = new Fl_Choice(150,530,60,30,"ch2");
  ch2_sel->align(FL_ALIGN_TOP);
  struct config_subword ch2_subword={"ch2", &conf, 4, 4, 0, 0, 0, 0.0, 1.0};
  struct menu_sel ch2_choices[] = {
	{"a",   0, &ch2_subword},
	{"b",   1, &ch2_subword},
	{"c",   2, &ch2_subword},
	{"d",   3, &ch2_subword},
	{"err", 4, &ch2_subword},
	{"fb",  5, &ch2_subword},
	{"cal", 6, &ch2_subword},
	{"pz",  7, &ch2_subword}};
  for (unsigned u=0; u<8; u++) {
	ch2_sel->add(ch2_choices[u].name, 0, menu_sel_callback, &(ch2_choices[u]), 0);
  }
  ch2_sel->value(3); /* default */

#if 0
  /* Final mux select */
  Fl_Choice *fm_sel = new Fl_Choice(80,530,70,30,"fm");
  fm_sel->align(FL_ALIGN_TOP);
  struct config_subword fm_subword={"fm", &conf, 8, 2, 0, 0};
  struct menu_sel fm_choices[] = {
	{"/1",   0, &fm_subword},
	{"/16",  1, &fm_subword},
	{"/256", 2, &fm_subword},
	{"raw",  3, &fm_subword}};
  for (unsigned u=0; u<4; u++) {
	fm_sel->add(fm_choices[u].name, 0, menu_sel_callback, &(fm_choices[u]), 0);
  }
  fm_sel->value(1); /* default */
#endif

#if 0
  /* Acoustic adjustment */
  Fl_Choice *aa_sel = new Fl_Choice(10,530,70,30,"aa");
  aa_sel->align(FL_ALIGN_TOP);
  struct config_subword aa_subword={"acou", &acou_a, 0, 8, 0, 0};
  struct menu_sel aa_choices[] = {
	{"rcos_d",  97, &aa_subword},  //  rd[ 1]
	{"audi_d",  98, &aa_subword},  //  rd[ 2]
	{"eset_b",  75, &aa_subword},  //  ra[11]
	{"audi_a",  78, &aa_subword}}; //  ra[14]
  for (unsigned u=0; u<4; u++) {
	aa_sel->add(aa_choices[u].name, 0, menu_sel_callback, &(aa_choices[u]), 0);
  }
  aa_sel->value(97); /* default */
#endif

  /* Slow ADC output */
  for (unsigned u=0; u<9; u++) {
	char *f=new char(2);
	f[0]='0'+u;
	if (u==8) f[0]='T';
	f[1]=0;
	sadc_out[u] = new Fl_Output(200,185+u*25,60,20,f); // findme 190 200
	sadc_out[u]->align(FL_ALIGN_LEFT);
  }
  /* Other status */
  other_out[0] = new Fl_Output(10,50,335,20,NULL);
  other_out[0]->textsize(10);
  for (unsigned u=1; u<4; u++) {
	other_out[u] = new Fl_Output(10,427+u*20,170,20,NULL);
	other_out[u]->textsize(12);
  }

  /* Acoustic message */
  for (unsigned u=0; u<4; u++) {
	acoustic_out[u] = new Fl_Output(450+u*110,600,90,20,NULL);
	acoustic_out[u]->textsize(12);
  }

  /* Current data filename display */
  current_dfile = new Fl_Output(10,600,130,20,NULL);
  current_dfile->textsize(12);

  cv = new CubeViewUI(350,10,580,580,"foo");

  window->end();


  Fl::visual(FL_DOUBLE|FL_INDEX);
  window->show(argc, argv);
  return Fl::run();
}
