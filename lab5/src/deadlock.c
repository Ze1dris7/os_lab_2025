#define _XOPEN_SOURCE 700

#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t barrier;

static void *first_thread(void *arg)
{
    (void)arg;

    pthread_mutex_lock(&A);
    pthread_barrier_wait(&barrier);

    puts("Thread 1 holds A and waits for B");
    fflush(stdout);
    pthread_mutex_lock(&B);

    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}

static void *second_thread(void *arg)
{
    (void)arg;

    pthread_mutex_lock(&B);
    pthread_barrier_wait(&barrier);

    puts("Thread 2 holds B and waits for A");
    fflush(stdout);
    pthread_mutex_lock(&A);

    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void)
{
    pthread_t first;
    pthread_t second;

    if (pthread_barrier_init(&barrier, NULL, 2) != 0) {
        fprintf(stderr, "Could not initialize the barrier\n");
        return 1;
    }

    if (pthread_create(&first, NULL, first_thread, NULL) != 0) {
        fprintf(stderr, "Could not create thread 1\n");
        return 1;
    }

    if (pthread_create(&second, NULL, second_thread, NULL) != 0) {
        fprintf(stderr, "Could not create thread 2\n");
        return 1;
    }

    pthread_join(first, NULL);
    pthread_join(second, NULL);
    return 0;
}