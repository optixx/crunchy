/* Strip header off of a Xilinx .bit file, to make
 * something that corresponds to the raw Altera .rbf file,
 * suitable for checksum and download with usrper.
 */

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned int uint32;
/* utilities involved in reading the Xilinx .bit file.
 * File format documented by Alan Nishioka <alann@accom.com>
 */
uint32 get_long(void)
{
	int c;
	uint32 l;
	if ((c = getchar()) == EOF) exit(1); l = c;
	if ((c = getchar()) == EOF) exit(1); l = l<<8 | c;
	if ((c = getchar()) == EOF) exit(1); l = l<<8 | c;
	if ((c = getchar()) == EOF) exit(1); l = l<<8 | c;
	return l;
}

int get_short(void)
{
	int c, l;
	if ((c = getchar()) == EOF) return -1; l = c;
	if ((c = getchar()) == EOF) return -1; l = l<<8 | c;
	return l;
}

void find_key(int key)
{
	int c;
	if ((c = getchar()) != key) {
		fprintf(stderr,"find_key found 0x%.2x instead of 0x%.2x\n",c,key);
		exit(1);
	}
}

void print_string(const char *name)
{
	int i, c, len;
	char *str;
	if ((len = get_short()) < 0) {
		fprintf(stderr,"print_string got EOF instead of length\n");
		exit(1);
	}
	str = malloc(len+1);
	if (str == NULL) {
		perror("print_string malloc");
		exit(1);
	}
	for (i=0; i<len; i++) {
		if ((c = getchar()) == EOF) {
			fprintf(stderr,"print_string found EOF in string of length %d\n", len);
			exit(1);
		}
		str[i] = c;
	}
	str[len] = '\0';
	printf("%s%s\n", name, str);
}

int load_main(const char *outfile_name)
{
	FILE *o;
	int i, c, len;
	if ((len = get_short()) < 0) exit(1);
	for (i=0; i<len; i++) {
		if ((c = getchar()) == EOF) exit(1);
	}
	find_key(0x00);
	find_key(0x01);
	find_key(0x61);
	print_string("design: ");
	find_key(0x62);
	print_string("part:   ");
	find_key(0x63);
	print_string("date:   ");
	find_key(0x64);
	print_string("time:   ");
	find_key(0x65);
	len = get_long();
	printf("length: %d\n", len);
	if (outfile_name == NULL) {
		for (i=0; i<len/4; i++) {
			printf(" 0x%.8x\n", get_long());
		}
	} else if ((o=fopen(outfile_name,"w"))!=NULL) {
		for (i=0; i<len; i++) {
			fputc(getchar(),o);
		}
		if (fclose(o)) {
			perror("fclose");
			unlink(outfile_name);
		} else {
			printf("wrote:  %s\n",outfile_name);
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	(void) argc;  /* unused */
	/* argument will be NULL if no file specified */
	load_main(argv[1]);
	return 0;
}
