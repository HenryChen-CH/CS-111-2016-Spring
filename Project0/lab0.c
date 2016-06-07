#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

extern char *optarg;

struct option long_options [] = {
	{"input", required_argument, 0, 'i'},
	{"output", required_argument, 0, 'o'},
	{"segfault", no_argument, 0, 's'},
	{"catch", no_argument, 0, 'c'},
	{0, 0, 0, 0}
};

void dump_handler(int signum);

int main(int argc, char** argv) {
	int option_index = 0;
	int c = 0;
	int n_cache = 100;

	char * input = 0;
	char * output = 0;
	int segfault = 0;
	int catch = 0;
	int fd0, fd1;
	char *buf = malloc(5*sizeof(char));
	size_t count = 0;
	char * tmp = 0;

	//parse the parameter
	while ((c = getopt_long(argc, argv, "i:o:sc", long_options, &option_index)) != -1) {
		switch (c) {
			case 'i' :
				if ((int)strlen(optarg) > 0) {
					input = malloc(strlen(optarg)*sizeof(char));
					strcpy(input, optarg);
				}
				break;
			case 'o' :
				if ((int)strlen(optarg) > 0) {
					output = malloc(strlen(optarg)*sizeof(char));
					strcpy(output, optarg);
				}
				break;
			case 's' :
				segfault = 1;
				break;
			case 'c' :
				catch = 1;
				break;
			case '?':
				break;
		}
	}

	if (catch) {
		signal(SIGSEGV, dump_handler);
	}

	if (segfault) {
		strcpy(tmp, "Dump!!");
	}

	//open file
	fd0 = open(input, O_RDONLY);
	if (fd0 == -1) {
		fprintf(stderr, "Input file open fail: %s\n", input);
		perror("");
		exit(1);
	}

	fd1 = creat(output, 0666);
	if (fd1 == -1) {
		close(fd0);
		fprintf(stderr, "Output file create fail: %s\n", output);
		perror("");	
		exit(2);	
	}

	close(0);
	dup(fd0);
	close(fd0);

	close(1);
	dup(fd1);
	close(fd1);
	//copy
	//
	count = read(0, buf, n_cache);
	while (count > 0) {
		write(1, buf, count);
		count = read(0, buf, n_cache);
	}
	close(0);
	close(1);

	return 0;
}

void dump_handler(int signum) {
	fprintf(stderr, "%s\n", "Segmentation fault caught!");
	exit(3);
}
