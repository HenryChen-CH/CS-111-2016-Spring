#include "SortedList.h"

#define _GNU_SOURCE
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
pthread_mutex_t *mutexes = NULL;
int *my_spin_locks = NULL;
int num_of_lists = 1;



struct option long_options[] = {
    {"threads", required_argument, 0, 't'},
    {"iterations", required_argument, 0, 'i'},
    {"yield", required_argument, 0, 'y'},
    {"sync", required_argument, 0, 's'},
    {"lists", required_argument, 0, 'l'},
    {0, 0, 0, 0}
};

char * alpha_string = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
extern char *optarg;
SortedList_t *list_heads;

void lock();
void unlock();
void *thread_run(void *elements);
void handle_error(char * msg, int flag);
char * random_key();
int hash_key(const char * string);

int main(int argc, char** argv) {
    int option_index = 0;
    int s = 0, c, res = 0;
    char * yield_opt;
    pthread_t *thread_id = NULL;
    pthread_attr_t attr;

    while ((c = getopt_long(argc, argv, "t:i:ys:", long_options, &option_index)) != -1) {
        switch (c) {
            case 't':
                num_of_threads = atoi (optarg);
                break;
            case 'i' :
                num_of_iterations = atoi (optarg);
                break;
            case 'y':
                yield_opt = strdup(optarg);
                break;
            case 's':
                lock_type = *optarg;
                break;
            case 'l':
                num_of_lists = atoi(optarg);
                break;
            default:
                break;
        }
    }
    if (num_of_lists <= 0) exit(1);

    char yield = 0x00;
    for (int i = 0; i < strlen(yield_opt); i++) {
        yield = yield_opt[i];
        switch (yield) {
            case 'i':
                opt_yield |= INSERT_YIELD;
                break;
            case 'd':
                opt_yield |= DELETE_YIELD;
                break;
            case 's':
                opt_yield |= SEARCH_YIELD;
                break;
        	default:
                break;
        }
    }
    // fprintf(stderr, "lock type: %c\n", lock_type);
    struct timespec my_start_time, my_end_time;

    long num_of_operations = num_of_iterations*num_of_threads;
    // fprintf(stderr, "number of operations: %ld\n", num_of_operations);
    SortedListElement_t * elements = malloc(num_of_operations*sizeof(SortedListElement_t));
    if (elements == NULL) handle_error("malloc() failed elements", 1);

    my_spin_locks = malloc(num_of_lists*sizeof(int));
    if (my_spin_locks == NULL) handle_error("malloc() failed", 1);

    mutexes = malloc(num_of_lists*sizeof(pthread_mutex_t));
    if (mutexes == NULL) handle_error("malloc() fail", 1);

    list_heads = malloc(num_of_lists*sizeof(SortedList_t));
    if (list_heads == NULL) handle_error("malloc() fail", 1);

    memset(my_spin_locks, 0, num_of_lists*sizeof(int));
    memset(mutexes, 0, num_of_lists*sizeof(pthread_mutex_t));
    memset(list_heads, 0, num_of_lists*sizeof(SortedList_t));
    memset(elements, 0, num_of_operations*sizeof(SortedListElement_t));

    for (int i = 0; i < num_of_lists; i++){
        pthread_mutex_init(mutexes+i, NULL);
        list_heads[i].prev = list_heads+i;
        list_heads[i].next = list_heads+i;
    }

    for (int i = 0; i < num_of_operations; i++) {
        elements[i].key = random_key();
    }

    thread_id = malloc(num_of_threads*sizeof(pthread_t));
    if (thread_id == NULL) handle_error("malloc() failed thread", 1);
    pthread_attr_init(&attr);

    clock_gettime (CLOCK_MONOTONIC, &my_start_time);

    for (int i = 0; i < num_of_threads; i++) {
        s = pthread_create(thread_id+i, &attr, thread_run, (void*)(elements+i*num_of_iterations));
        if (s != 0) handle_error("pthread_create() faile", 1);
    }
    // fprintf(stderr, "%s\n", "creat finish");
    for (int i = 0; i < num_of_threads; i++) {
        s = pthread_join(thread_id[i], (void **)&res);
        if (s != 0) handle_error ("pthread_join() failed", 1);
        if (res != 0) fprintf(stderr, "%s\n", "pthread return value error");
    }

    clock_gettime (CLOCK_MONOTONIC, &my_end_time);

    int len = 0;
    for (int i = 0; i < num_of_lists; i++) {
        len += SortedList_length(list_heads+i);
    }
    if (len != 0) fprintf(stderr, "ERROT: len is %d\n", len);

	long long my_elapsed_time_in_ns = (my_end_time.tv_sec - my_start_time.tv_sec) * 1000000000;
	my_elapsed_time_in_ns += my_end_time.tv_nsec;
	my_elapsed_time_in_ns -= my_start_time.tv_nsec;

    long long total_operations = num_of_iterations*num_of_threads*2;
    fprintf(stdout, "%d threads x %d iterations x (insert + lookup/delete) = %lld \n", num_of_threads,
        num_of_iterations, total_operations);
    fprintf(stdout, "elapsed time: %lldns\n", my_elapsed_time_in_ns);
    fprintf(stdout, "per operation: %lldns\n", my_elapsed_time_in_ns/total_operations);
    fprintf(stdout, "corrected average time: %fns\n", ((float)(my_elapsed_time_in_ns))/total_operations/total_operations*2*num_of_lists);


    free(thread_id);
    for (int i = 0; i < num_of_operations; i++) {
        free((void*)(elements[i].key));
    }
    free((void*)elements);
    if (len != 0) return 1;
    return 0;

}

void *thread_run(void *elements){
    SortedListElement_t * nodes = (SortedListElement_t*)elements;
    //fprintf(stderr, "%s\n", "thread start");
    int hash = 0, sum = 0;
    for (int i = 0; i < num_of_iterations; i++) {
        // fprintf(stderr, "insert key is %s\n", (nodes+i)->key);
        hash = hash_key(nodes[i].key) % num_of_lists;
        lock(hash);
        SortedList_insert(list_heads+hash, nodes+i);
        unlock(hash);
    }

    for (int i = 0; i < num_of_lists; i++) {
        lock(i);
        sum += SortedList_length(list_heads+i);
    }

    for (int i = 0; i < num_of_lists; i++) {
        unlock(i);
    }

    SortedListElement_t * found = NULL;

    for (int i = 0; i < num_of_iterations; i++) {
        hash = hash_key(nodes[i].key) % num_of_lists;
        lock(hash);
        found = SortedList_lookup(list_heads+hash, (nodes+i)->key);
        SortedList_delete(found);
        unlock(hash);
    }
    pthread_exit(0);
}

void handle_error(char *msg, int flag) {
    perror(msg);
    exit(flag);
}

char *random_key() {
    int len = random()%10+1;
    char *s = malloc((len+1)*sizeof(char));
    memset((void *)s, 0, (len+1)*sizeof(char));

    for (int i = 0; i < len; i++) {
        s[i] = alpha_string[random()%52];
    }
    s[len] = '\0';
    //fprintf(stderr, "generated key is %s\n", s);
    return s;
}

void lock(int i) {
    switch (lock_type) {
        case 's':
            while (__sync_lock_test_and_set(my_spin_locks+i, 1));
            break;
        case 'm':
            pthread_mutex_lock(mutexes+i);
            break;
        default:
            break;
    }
}

void unlock(int i) {
    switch (lock_type) {
        case 's':
            __sync_lock_release(my_spin_locks+i);
            break;
        case 'm':
            pthread_mutex_unlock(mutexes+i);
            break;
        default:
            break;
    }
}

int hash_key(const char * string) {
    int len = strlen(string);
    if (len > 0) {
        if (len > 1) return string[0]+string[1];
        return string[0];
    }
    return 31;
}
