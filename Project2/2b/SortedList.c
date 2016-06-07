#define _GNU_SOURCE
#include "SortedList.h"
#include <pthread.h>
#include <string.h>

extern int num_of_threads;
extern int num_of_iterations;
extern int opt_yield;


// Insert
void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
    if (element->key == NULL) return;
    SortedListElement_t *previous = list;
    SortedListElement_t *next = list->next;

    while (next != list) {
        // increasing order
        if (strcmp(element->key, next->key) <= 0) break;

        previous = next;
        next = next->next;
    }

    if (opt_yield & INSERT_YIELD) pthread_yield();

    element->prev = previous;
    element->next = next;
    previous->next = element;
    next->prev = element;

}

// Delete
int SortedList_delete(SortedListElement_t *element) {
    if (opt_yield && DELETE_YIELD) pthread_yield();

    if (element->next->prev == element && element->prev->next == element) {
        element->prev->next = element->next;
        element->next->prev = element->prev;

        return 0;
    }
    return 1;
}

// Lookup
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
    SortedListElement_t * next = list->next;
    if (opt_yield && SEARCH_YIELD) pthread_yield();

    while (next != list) {
        if (next->next->prev != next || next->prev->next != next) {
            return NULL;
        }

        if (strcmp(key, next->key) == 0) {
            return next;
        }
        next = next->next;
    }

    return NULL;
}

// Length
int SortedList_length(SortedList_t *list) {
    SortedListElement_t * next = list->next;
    int len = 0;

    if (opt_yield && SEARCH_YIELD) pthread_yield();

    while (next != list) {
        if (next->prev->next != next || next->next->prev != next) {
            return -1;
        }
        len++;
        next = next->next;
    }
    return len;
}
