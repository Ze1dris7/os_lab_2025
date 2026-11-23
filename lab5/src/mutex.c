#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* Общий счётчик для двух потоков. */
static int common = 0;

/* Мьютекс включается при сборке с флагом -DUSE_MUTEX. */
#ifdef USE_MUTEX
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void *increment(void *arg)
{
    (void)arg;

    for (int i = 0; i < 50; i++) {
#ifdef USE_MUTEX
        pthread_mutex_lock(&mutex);
#endif

        /* Начало критической секции. */
        int work = common;
        work++;

        /* Задержка делает потерянные обновления заметнее. */
        for (volatile unsigned long delay = 0;
             delay < 500000;
             delay++) {
        }

        common = work;
        /* Конец критической секции. */

#ifdef USE_MUTEX
        pthread_mutex_unlock(&mutex);
#endif
    }

    return NULL;
}

int main(void)
{
    pthread_t first;
    pthread_t second;

    int error = pthread_create(&first, NULL, increment, NULL);
    if (error != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        return 1;
    }

    error = pthread_create(&second, NULL, increment, NULL);
    if (error != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        return 1;
    }

    error = pthread_join(first, NULL);
    if (error != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(error));
        return 1;
    }

    error = pthread_join(second, NULL);
    if (error != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(error));
        return 1;
    }

    printf("All done, counter = %d\n", common);

    return 0;
}