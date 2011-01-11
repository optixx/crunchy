#ifndef RFILE_UTIL_H
#define RFILE_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

int llrf_read_header(const char *fname, int verbose);
void llrf_unmap(void);
unsigned int llrf_fill_data(double **xdata_p);
double llrf_vscale(void);
double llrf_tscale(void);
void llrf_print_fast(void);
void llrf_print_raw(void);
void llrf_print_slow(void word_handler(unsigned v, FILE *f, unsigned *));
void llrf_print_sets(void);
void llrf_print_acoustic(void);

#ifdef __cplusplus
}
#endif

#endif
