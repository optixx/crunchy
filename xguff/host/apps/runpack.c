#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "rfile_util.h"

/* skip the include file for a single declaration */
void aux_check_word(unsigned int v, FILE *print, unsigned *burst_age);

/* ============================================
 * Main program: Unix-style print data to stdout
 * ============================================
 */
int main(int argc, char **argv)
{
	int i, rc;

	if (argc < 2) {
		fprintf(stderr,"usage: %s filename [slow|fast|raw|sets|acoustic]\n",argv[0]);
		return 1;
	}
	rc = llrf_read_header(argv[1], 1);
	if (rc) return rc;

	for (i=2; i<argc; i++) {
		     if (strcasecmp(argv[i],"slow"    )==0) llrf_print_slow (aux_check_word);
		else if (strcasecmp(argv[i],"fast"    )==0) llrf_print_fast ();
		else if (strcasecmp(argv[i],"raw"     )==0) llrf_print_raw  ();
		else if (strcasecmp(argv[i],"sets"    )==0) llrf_print_sets ();
		else if (strcasecmp(argv[i],"acoustic")==0) llrf_print_acoustic ();
		else fprintf(stderr,"unknown option: %s\n", argv[i]);
	}
	return 0;
}
