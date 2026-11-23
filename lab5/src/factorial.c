#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Task {
    pthread_t thread;
    long long first;
    long long last;
};

static int mod;
static int result;
static pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

static int read_number(const char *text)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < 0 || value > INT_MAX) {
        return -1;
    }

    return (int)value;
}

static void *multiply_range(void *arg)
{
    const struct Task *task = arg;
    int local = 1 % mod;

    for (long long i = task->first; i <= task->last; ++i) {
        local = 1LL * local * i % mod;
    }

    pthread_mutex_lock(&result_mutex);
    result = 1LL * result * local % mod;
    pthread_mutex_unlock(&result_mutex);

    return NULL;
}

int main(int argc, char *argv[])
{
    int k = -1;
    int pnum = -1;
    static const struct option options[] = {
        {"pnum", required_argument, NULL, 'p'},
        {"mod", required_argument, NULL, 'm'},
        {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "k:", options, NULL)) != -1) {
        switch (option) {
        case 'k':
            k = read_number(optarg);
            break;
        case 'p':
            pnum = read_number(optarg);
            break;
        case 'm':
            mod = read_number(optarg);
            break;
        default:
            return 1;
        }
    }

    if (k < 0 || pnum <= 0 || mod <= 0 || optind != argc) {
        fprintf(stderr, "Usage: %s -k NUMBER --pnum=THREADS --mod=MOD\n",
                argv[0]);
        return 1;
    }

    struct Task *tasks = calloc((size_t)pnum, sizeof *tasks);
    if (tasks == NULL) {
        perror("calloc");
        return 1;
    }

    result = 1 % mod;
    int per_thread = k / pnum;
    int remainder = k % pnum;
    long long next = 1;
    int started = 0;

    for (; started < pnum; ++started) {
        int count = per_thread;
        if (started < remainder) {
            ++count;
        }

        tasks[started].first = next;
        tasks[started].last = next + count - 1;
        next += count;

        int error = pthread_create(&tasks[started].thread, NULL,
                                   multiply_range, &tasks[started]);
        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            break;
        }
    }

    for (int i = 0; i < started; ++i) {
        int error = pthread_join(tasks[i].thread, NULL);
        if (error != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            return 1;
        }
    }

    free(tasks);

    if (started != pnum) {
        return 1;
    }

    printf("%d\n", result);
    return 0;
}