#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <getopt.h>
#include <pthread.h>
#include <time.h>

extern char *optarg;
struct option long_options[] = {
	{"threads", required_argument, 0, 't'},
	{"iterations", required_argument, 0, 'i'},
	{"yield", no_argument, 0, 'y'},
	{"sync", required_argument, 0, 's'},
	{0, 0, 0, 0}
};


char clock_type = 0x00;
long long counter = 0;

int num_of_threads = 1;
int num_of_iterations = 1;

int opt_yield = 0;

pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER; 
int my_spin_lock = 0;

void add (long long * pointer, long long value);
void handle_error (char * mes, int flag);
void thread_add(long long value);

void *thread_run();


int main(int argc, char** argv) {
	int option_index = 0;
	int s = 0, c;
	int res = 0;

	pthread_t *threads_id = NULL;
	pthread_attr_t attr;

	struct timespec my_start_time, my_end_time;

	while ((c = getopt_long(argc, argv, "t:i:ys:", long_options, &option_index)) != -1) {
		switch (c) {
			case 't':
				num_of_threads = atoi (optarg);
				break;
			case 'i' :
				num_of_iterations = atoi (optarg);
				break;
			case 'y':
				opt_yield = 1;
				break;
			case 's':
				clock_type = *optarg;
				break;
			default:
				break;
		}
	}

	if (num_of_iterations <= 0 || num_of_threads <= 0) {
		fprintf(stderr, "%s\n", "Invalid input");
		exit(1);
	}

	
	s = pthread_attr_init(&attr);
	if (s != 0) handle_error ("pthread_attr_init() failed", 1);

	threads_id = malloc (num_of_threads*sizeof(pthread_t));
	if (threads_id == NULL) handle_error ("malloc() failed", 1);
	
	clock_gettime (CLOCK_MONOTONIC, &my_start_time);
	for (int i = 0; i < num_of_threads; i++) {
		s = pthread_create(threads_id+i, &attr, thread_run, NULL);
		if (s != 0) handle_error ("pthread_create() failed", 1);
	}

	for (int i = 0; i < num_of_threads; i++) {
		s = pthread_join (threads_id[i], (void **)&res);
		if (s != 0) handle_error ("pthread_join() failed", 1);
		if (res != 0) fprintf(stderr, "%s\n", "pthread return value error");
	}

	clock_gettime (CLOCK_MONOTONIC, &my_end_time);
	long long my_elapsed_time_in_ns = (my_end_time.tv_sec - my_start_time.tv_sec) * 1000000000;
	my_elapsed_time_in_ns += my_end_time.tv_nsec;
	my_elapsed_time_in_ns -= my_start_time.tv_nsec;

	if (counter != 0) {
		fprintf(stderr, "ERROR: final count = %lld\n", counter);
	}
	long long total_operations = num_of_iterations*num_of_threads*2;
	fprintf(stdout, "%d threads x %d iterations x (add + subtract) = %lld \n", num_of_threads,
		num_of_iterations, total_operations);
	fprintf(stdout, "elapsed time: %lldns\n", my_elapsed_time_in_ns);
	fprintf(stdout, "per operation: %lldns\n", my_elapsed_time_in_ns/total_operations);

	free(threads_id);
	if (counter != 0) return 1;
	return 0;

}

void *thread_run() {
	for (int i = 0; i < num_of_iterations; i++) {
		thread_add(1);
	}

	for (int i = 0; i < num_of_iterations; i++) {
		thread_add(-1);
	}

	pthread_exit(0);
};

void thread_add(long long value) {
	long long previous, sum;
	switch (clock_type) {
		case 'm':
			pthread_mutex_lock(&my_mutex);
			add(&counter, value);
			pthread_mutex_unlock(&my_mutex);
			break;

		case 's':
			while (__sync_lock_test_and_set(&my_spin_lock, 1));
			add(&counter, value);
			__sync_lock_release(&my_spin_lock);
			break;

		case 'c':
			do {
				previous = counter;
				sum = previous + value;				
			} while (!__sync_bool_compare_and_swap(&counter, previous, sum));
			break;

		default:
			add(&counter, value);
			break;
	}
}

void add (long long * pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield) pthread_yield ();
	*pointer = sum;
}

void handle_error (char * msg, int flag) {
	perror (msg);
	exit (flag);
}

