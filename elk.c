#include "elk.h"
int MEM_INDEX = 0;
int TX_MEM_SIZE = 0;
void **TX_MEM;
size_t *TX_MEM_ELEMENT_SIZES;

int THREAD_NUM;
ThreadData *THREAD_DATA;
pthread_rwlock_t *RW_LOCKS;

pthread_t get_thread_id() {
    //placeholder
    return pthread_self();
}

ThreadData *get_thread_data() {
    //TODO: use thread_specific variables. See man pthread for more info.
    pthread_t thread_id = get_thread_id();
    for (int i = 0; i < THREAD_NUM; i++) {
        if (thread_id == THREAD_DATA[i].thread_id) {
            return &THREAD_DATA[i];
        }
    }
    printf("Thread not found, internal error!\n");
    exit(1);
}
ThreadData *get_thread_data_no_err() {
    pthread_t thread_id = get_thread_id();
    for (int i = 0; i < THREAD_NUM; i++) {
        if (thread_id == THREAD_DATA[i].thread_id) {
            return &THREAD_DATA[i];
        }
    }
    return NULL;
}
void *salloc(size_t size) {
    void *ret = malloc(size);
    if (ret == NULL) {
        printf("Elk failed to allocate tx_mem, exiting.\n");
        exit(1);
    }
    return ret;
}

bool elk_init() {
    TX_MEM_SIZE = 100;
    TX_MEM = salloc(TX_MEM_SIZE * sizeof(void *));
    RW_LOCKS = salloc(TX_MEM_SIZE * sizeof(pthread_rwlock_t));
    for (int i = 0; i < TX_MEM_SIZE; i++) {
        pthread_rwlock_init(&RW_LOCKS[i], NULL);
    }
    TX_MEM_ELEMENT_SIZES = salloc(TX_MEM_SIZE * sizeof(size_t));
    for (int i = 0; i < TX_MEM_SIZE; i++) {
        TX_MEM_ELEMENT_SIZES[i] = 0;
    }
    THREAD_NUM = 10;
    THREAD_DATA = salloc(THREAD_NUM * sizeof(ThreadData));
    for (int i = 0; i < THREAD_NUM; i++) {
        THREAD_DATA[i].thread_id = NULL;
        //change to during tx_start to allow for memory space expansion
        THREAD_DATA[i].local_mem = salloc(TX_MEM_SIZE * sizeof(void *));
        THREAD_DATA[i].local_old_mem = salloc(TX_MEM_SIZE * sizeof(void *));
        THREAD_DATA[i].in_tx = false;
    }
    return true;
}
void tx_initialize_threadlocal_memory(ThreadData *to_write) {
    //copy over all our info
    int err;
    for (int i = 0; i < TX_MEM_SIZE; i++) {
        if ((err = pthread_rwlock_rdlock(&RW_LOCKS[i]))) {
            printf("Error occured while acquiring read-write lock! Error code: %i\n", err);
            exit(1);
        }
        to_write->local_mem[i] = salloc(TX_MEM_ELEMENT_SIZES[i]);
        to_write->local_old_mem[i] = salloc(TX_MEM_ELEMENT_SIZES[i]);
        memmove(to_write->local_mem[i], TX_MEM[i], TX_MEM_ELEMENT_SIZES[i]);
        memmove(to_write->local_old_mem[i], TX_MEM[i], TX_MEM_ELEMENT_SIZES[i]);
        pthread_rwlock_unlock(&RW_LOCKS[i]);
    }
}
void tx_cleanup_threadlocal_memory(ThreadData *data) {
    for (int i = 0; i < TX_MEM_SIZE; i++) {
        free(data->local_mem[i]);
        free(data->local_old_mem[i]);
    }
}

void tx_start(pthread_t thread_id) {
    ThreadData *to_write = NULL;
    bool found = false;
    for (int i = 0; i < THREAD_NUM; i++) {
        if (thread_id == THREAD_DATA[i].thread_id) {
            found = true;
            to_write = &THREAD_DATA[i];
            to_write->thread_id = thread_id;
            if (to_write->in_tx) {
                printf("Entered transaction while in transaction!\n");
                exit(1);
            }
            to_write->in_tx = true;
            tx_initialize_threadlocal_memory(to_write);
            return;
        }
        if (THREAD_DATA[i].thread_id == NULL && to_write == NULL) {
            //keep potential location to write to in case this thread
            //hasn't been documented yet
            to_write = &THREAD_DATA[i];
        }
    }
    if (!found) {
        to_write->thread_id = thread_id;
        to_write->in_tx = true;
        tx_initialize_threadlocal_memory(to_write);
    } 
}

struct TestPair {
    tx_mem_t value1;
    tx_mem_t value2;
};
typedef struct TestPair TestPair;
void *increment_test_transaction(void *t) {
    TestPair *test_pair = t;
    tx_mem_t value2 = test_pair->value2;
    int to_inc;
    for (int i = 0; i < 5; i++) { 
        tx_read(value2, &to_inc, sizeof(int));
        sleep(1);
        to_inc++;
        printf("Writing %i to memory.\n", to_inc);
        tx_write(value2, &to_inc, sizeof(int)); 
    }
    return NULL;
}
void *test_set_values(void *t) {
    TestPair *test_pair = t;
    tx_mem_t value1 = test_pair->value1;
    tx_mem_t value2 = test_pair->value2;
    int initial = 0;
    tx_write(value2, &initial, sizeof(int));
    return NULL;
}

void *test(void *t) {
    TestPair *test_pair = t;
    tx_mem_t value1 = test_pair->value1;
    tx_mem_t value2 = test_pair->value2;
    int four = 4;
    tx_write(value1, "test", 5 * sizeof(char));
    tx_write(value2, &four, sizeof(int));
    char *val1 = salloc(5 *sizeof(char));
    tx_read(value1, val1, 5 *sizeof(char));
    printf("%s\n", val1);
    int val2;
    tx_read(value2, &val2, sizeof(int));
    printf("%i\n", val2); 
    return NULL;
}

void *test2(void *t) {
    TestPair *test_pair = t;
    tx_mem_t value1 = test_pair->value1;

    void *val = salloc(5 * sizeof(char));
    tx_read(value1, val, 5 * sizeof(char));
    return val;
}
void *increment_test(void *in) {
    TestPair *tp = in;
    return execute(increment_test_transaction, tp); 
}
int main() {
    elk_init();
    TestPair *test_pair = salloc(sizeof(TestPair)); 
    test_pair->value1 = tx_alloc(5 * sizeof(char));
    test_pair->value2 = tx_alloc(sizeof(int));
    execute(test_set_values, test_pair);
    pthread_t t1;
    pthread_t t2;
    pthread_create(&t1, NULL, increment_test, test_pair);
    pthread_create(&t2, NULL, increment_test, test_pair);
    void *ret;
    pthread_join(t1, ret);
    pthread_join(t2, ret);
}
void tx_abort(ThreadData *data, int i) {
    //lose all locks
    data->in_tx = false;
    for (; i >= 0; i--) {
        pthread_rwlock_unlock(&RW_LOCKS[i]);
    }

}
bool tx_commit() {
    ThreadData *data = get_thread_data();
    int err;
    //TODO need to acquire locks here, later
    for (int i = 0; i < TX_MEM_SIZE; i++) {
        //try to acquire lock, fail if doesn't work
        if ((err = pthread_rwlock_trywrlock(&RW_LOCKS[i]))) {
            printf("Error acquiring locks!\n");
            tx_abort(data, i - 1);
            return false;
        } 
        //compare bytes to determine if changed
        uint8_t *old = data->local_old_mem[i];
        uint8_t *new = TX_MEM[i];
        size_t cur_size = 0;
        for (; cur_size < TX_MEM_ELEMENT_SIZES[i]; cur_size += sizeof(uint8_t), old++, new++) {
            if (*old != *new) {
                printf("Memory mismatch!\n");
                tx_abort(data, i);
                return false;
            }
        }
    }
    //commit copy
    for (int i = 0; i < TX_MEM_SIZE; i++) {
        memmove(TX_MEM[i], data->local_mem[i], TX_MEM_ELEMENT_SIZES[i]);
        pthread_rwlock_unlock(&RW_LOCKS[i]);
    }
    data->in_tx = false;
    tx_cleanup_threadlocal_memory(data);
    return true;
}
void *execute(void *(*f)(void *), void *arg) {
    tx_start(get_thread_id());
    void *ret = f(arg);
    while (!tx_commit()) { 
        tx_start(get_thread_id());
        ret = f(arg);
    }
    return ret;
}

tx_mem_t tx_alloc(size_t size) {
    ThreadData *data = get_thread_data_no_err();
    if (data != NULL && data->in_tx) {
        printf("Error! Attempted tx_alloc while in a transaction. Exiting.\n");
        exit(1);
    }

    void *mem = salloc(size);
    TX_MEM[MEM_INDEX] = mem;  
    TX_MEM_ELEMENT_SIZES[MEM_INDEX] = size;
    MEM_INDEX++;
    return MEM_INDEX - 1;
}

void tx_write(tx_mem_t id, void *new_value, size_t size) {
    if (size > TX_MEM_ELEMENT_SIZES[id]) {
        printf("Attempting to write beyond size of element!\n"); 
        exit(1);
    }

    ThreadData *data = get_thread_data(); 
    if (!data->in_tx) {
        printf("Error! Attempted tx_write while not in a transaction. Exiting.\n");
        exit(1);
    }
    if (data->local_mem[id] == NULL) {
        printf("Attempting to write to non-allocated memory!\n");
        exit(1);
    }
    memmove(data->local_mem[id], new_value, size);
    //memmove(TX_MEM[id], new_value, size);
}

void tx_read(int id, void *out, size_t amount_to_read) {
    //tx_read should copy value into provided space instead of returning a pointer  
    //return TX_MEM[id];
    if (amount_to_read > TX_MEM_ELEMENT_SIZES[id]) {
        printf("Attempting to read beyond size of element!\n");
        exit(1);
    }
    ThreadData *data = get_thread_data(); 
    if (!data->in_tx) {
        printf("Error! Attempted tx_read while not in a transaction. Exiting.\n");
        exit(1);
    }
    if (data->local_mem[id] == NULL) {
        printf("Attempting to read non-allocated memory!\n");
        exit(1);
    }
    void *read_data = data->local_mem[id];
    memmove(out, read_data, amount_to_read); 
}
