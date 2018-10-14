#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>


void *doTheThing(void *vargp) {
    sleep(1);
    printf("In thread\n");
    return NULL;
}

int main() {
    pthread_t thread_id;
    printf("Before thread\n");
    pthread_create(&thread_id, NULL, doTheThing, NULL);
    printf("In between thread.\n");
    pthread_join(thread_id, NULL);
    printf("After thread\n");
    return 0;
}
