#ifndef ELK_H
#define ELK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

typedef int tx_mem_t;

void *execute(void *(*f)(void *), void *arg);

tx_mem_t tx_alloc(size_t size);

void tx_write(int id, void *new_value, size_t size);

void tx_read(int id, void *out_val, size_t size); 

bool elk_init();

struct ThreadData {
    pthread_t thread_id;
    void **local_mem; 
    void **local_old_mem;
    bool in_tx;
};

typedef struct ThreadData ThreadData;

#endif
