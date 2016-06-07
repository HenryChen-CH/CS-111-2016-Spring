#ifndef HEAD
#define HEAD

#define _GNU_SOURCE
//#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <getopt.h>
#include <pthread.h>
#include <time.h>

#include <string.h>

int num_of_threads = 1;
int num_of_iterations = 1;

int opt_yield = 0;


char lock_type = 0x00;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int my_spin_lock = 0;

#endif
