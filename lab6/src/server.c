#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  pthread_t thread;
};

static void *ThreadFactorial(void *argument) {
  struct FactorialArgs *args = argument;
  uint64_t result = 1 % args->mod;

  for (uint64_t n = args->begin; result != 0; ++n) {
    result = MultModulo(result, n, args->mod);
    if (n == args->end) {
      break;
    }
  }
  args->result = result;
  return NULL;
}

static bool ComputeRange(uint64_t begin, uint64_t end, uint64_t mod,
                         size_t count, uint64_t *total) {
  uint64_t length = end - begin + 1;
  if (length < count) {
    count = (size_t)length;
  }
  struct FactorialArgs *args = calloc(count, sizeof(*args));
  if (args == NULL) {
    perror("calloc");
    return false;
  }

  const uint64_t base = length / count;
  const uint64_t extra = length % count;
  uint64_t last = begin - 1;
  size_t started = 0;

  for (; started < count; ++started) {
    args[started].begin = last + 1;
    last += base + (started < extra ? 1 : 0);
    args[started].end = last;
    args[started].mod = mod;
    int error =
        pthread_create(&args[started].thread, NULL, ThreadFactorial, &args[started]);
    if (error != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      break;
    }
  }

  *total = 1 % mod;
  for (size_t i = 0; i < started; ++i) {
    pthread_join(args[i].thread, NULL);
    *total = MultModulo(*total, args[i].result, mod);
  }
  free(args);
  return started == count;
}

static void ServeClient(int fd, size_t threads) {
  for (;;) {
    uint64_t request[3];
    int status = RecvAll(fd, request, sizeof(request));
    if (status <= 0) {
      if (status < 0) {
        perror("recv");
      }
      return;
    }

    uint64_t begin = request[0];
    uint64_t end = request[1];
    uint64_t mod = request[2];
    if (begin == 0 || end < begin || mod == 0) {
      fprintf(stderr, "Invalid range or modulus\n");
      return;
    }

    uint64_t result;
    if (!ComputeRange(begin, end, mod, threads, &result)) {
      return;
    }
    if (!SendAll(fd, &result, sizeof(result))) {
      perror("send");
      return;
    }
  }
}

int main(int argc, char **argv) {
  static const struct option options[] = {
      {"port", required_argument, NULL, 'p'},
      {"tnum", required_argument, NULL, 't'},
      {0, 0, 0, 0},
  };
  uint64_t port = 0;
  uint64_t threads = 0;
  int option;

  while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (option) {
    case 'p':
      if (!ParseUInt64(optarg, &port) || port == 0 || port > UINT16_MAX) {
        fprintf(stderr, "Invalid port: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    case 't':
      if (!ParseUInt64(optarg, &threads) || threads == 0 || threads > INT_MAX) {
        fprintf(stderr, "Invalid thread count: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    default:
      return EXIT_FAILURE;
    }
  }
  if (port == 0 || threads == 0 || optind != argc) {
    fprintf(stderr, "Usage: %s --port PORT --tnum THREADS\n", argv[0]);
    return EXIT_FAILURE;
  }

  signal(SIGPIPE, SIG_IGN);
  int listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  int reuse = 1;
  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons((uint16_t)port),
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
  };
  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    perror("setsockopt");
  } else if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
  } else if (listen(listener, 128) < 0) {
    perror("listen");
  } else {
    printf("Server listening at %" PRIu64 "\n", port);
    fflush(stdout);

    for (;;) {
      int client = accept(listener, NULL, NULL);
      if (client >= 0) {
        ServeClient(client, (size_t)threads);
        close(client);
      } else if (errno != EINTR && errno != ECONNABORTED) {
        perror("accept");
        break;
      }
    }
  }

  close(listener);
  return EXIT_FAILURE;
}
