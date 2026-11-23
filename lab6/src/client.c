#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct ServerJob {
  struct sockaddr_in address;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  bool success;
  pthread_t thread;
};

static uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  return (uint64_t)((__uint128_t)a * b % mod);
}

static bool ParseUInt64(const char *text, uint64_t *value) {
  if (*text < '0' || *text > '9') {
    return false;
  }

  char *end;
  errno = 0;
  uintmax_t parsed = strtoumax(text, &end, 10);
  if (errno == ERANGE || *end != '\0' || parsed > UINT64_MAX) {
    return false;
  }

  *value = (uint64_t)parsed;
  return true;
}

static struct ServerJob *ReadServers(const char *path, size_t *count) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    perror(path);
    return NULL;
  }

  struct ServerJob *jobs = NULL;
  size_t capacity = 0;
  char *line = NULL;
  size_t line_capacity = 0;
  size_t line_number = 0;

  while (getline(&line, &line_capacity, file) >= 0) {
    ++line_number;
    char *entry = line + strspn(line, " \t");
    size_t length = strlen(entry);
    while (length > 0 && strchr(" \t\r\n", entry[length - 1]) != NULL) {
      entry[--length] = '\0';
    }
    if (length == 0) {
      continue;
    }

    char *separator = strchr(entry, ':');
    uint64_t port;
    struct sockaddr_in address = {.sin_family = AF_INET};
    if (separator == NULL || !ParseUInt64(separator + 1, &port) ||
        port == 0 || port > UINT16_MAX) {
      fprintf(stderr, "%s:%zu: expected IPv4:port\n", path, line_number);
      goto error;
    }
    *separator = '\0';
    if (inet_pton(AF_INET, entry, &address.sin_addr) != 1) {
      fprintf(stderr, "%s:%zu: invalid IPv4 address\n", path, line_number);
      goto error;
    }
    address.sin_port = htons((uint16_t)port);

    if (*count == capacity) {
      if (capacity > SIZE_MAX / sizeof(*jobs) / 2) {
        fprintf(stderr, "Too many servers\n");
        goto error;
      }
      size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
      struct ServerJob *new_jobs = realloc(jobs, new_capacity * sizeof(*jobs));
      if (new_jobs == NULL) {
        perror("realloc");
        goto error;
      }
      jobs = new_jobs;
      capacity = new_capacity;
    }
    jobs[*count] = (struct ServerJob){.address = address};
    ++*count;
  }

  if (!feof(file) || ferror(file)) {
    perror(path);
    goto error;
  }
  if (*count == 0) {
    fprintf(stderr, "%s: server list is empty\n", path);
    goto error;
  }

  free(line);
  fclose(file);
  return jobs;

error:
  free(jobs);
  free(line);
  fclose(file);
  return NULL;
}

static bool SendAll(int fd, const void *buffer, size_t size) {
  const char *bytes = buffer;
  size_t offset = 0;
  while (offset < size) {
    ssize_t sent = send(fd, bytes + offset, size - offset, 0);
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    if (sent <= 0) {
      if (sent == 0) {
        errno = EPIPE;
      }
      return false;
    }
    offset += (size_t)sent;
  }
  return true;
}

static bool RecvAll(int fd, void *buffer, size_t size) {
  char *bytes = buffer;
  size_t offset = 0;
  while (offset < size) {
    ssize_t received = recv(fd, bytes + offset, size - offset, 0);
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received <= 0) {
      if (received == 0) {
        errno = ECONNRESET;
      }
      return false;
    }
    offset += (size_t)received;
  }
  return true;
}

static void *RunServerJob(void *argument) {
  struct ServerJob *job = argument;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return NULL;
  }

  uint64_t request[] = {job->begin, job->end, job->mod};
  if (connect(fd, (const struct sockaddr *)&job->address,
              sizeof(job->address)) < 0) {
    perror("connect");
  } else if (!SendAll(fd, request, sizeof(request))) {
    perror("send");
  } else if (!RecvAll(fd, &job->result, sizeof(job->result))) {
    perror("recv");
  } else {
    job->success = true;
  }
  close(fd);
  return NULL;
}

int main(int argc, char **argv) {
  static const struct option options[] = {
      {"k", required_argument, NULL, 'k'},
      {"mod", required_argument, NULL, 'm'},
      {"servers", required_argument, NULL, 's'},
      {0, 0, 0, 0},
  };
  uint64_t k = 0;
  uint64_t mod = 0;
  bool have_k = false;
  const char *path = NULL;
  int option;

  while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (option) {
    case 'k':
      if (!ParseUInt64(optarg, &k)) {
        fprintf(stderr, "Invalid k: %s\n", optarg);
        return EXIT_FAILURE;
      }
      have_k = true;
      break;
    case 'm':
      if (!ParseUInt64(optarg, &mod) || mod == 0) {
        fprintf(stderr, "Invalid modulus: %s\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    case 's':
      path = optarg;
      break;
    default:
      return EXIT_FAILURE;
    }
  }
  if (!have_k || mod == 0 || path == NULL || optind != argc) {
    fprintf(stderr, "Usage: %s --k K --mod MOD --servers FILE\n", argv[0]);
    return EXIT_FAILURE;
  }

  size_t count = 0;
  struct ServerJob *jobs = ReadServers(path, &count);
  if (jobs == NULL) {
    return EXIT_FAILURE;
  }
  uint64_t answer = 1 % mod;
  if (k == 0) {
    printf("answer: %" PRIu64 "\n", answer);
    free(jobs);
    return EXIT_SUCCESS;
  }

  signal(SIGPIPE, SIG_IGN);
  if (k < count) {
    count = (size_t)k;
  }
  const uint64_t base = k / count;
  const uint64_t extra = k % count;
  uint64_t end = 0;
  size_t started = 0;

  // Launch every connection before waiting for the first response.
  for (; started < count; ++started) {
    struct ServerJob *job = &jobs[started];
    job->begin = end + 1;
    end += base + (started < extra ? 1 : 0);
    job->end = end;
    job->mod = mod;
    int error = pthread_create(&job->thread, NULL, RunServerJob, job);
    if (error != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      break;
    }
  }

  bool success = started == count;
  for (size_t i = 0; i < started; ++i) {
    pthread_join(jobs[i].thread, NULL);
    if (jobs[i].success) {
      answer = MultModulo(answer, jobs[i].result, mod);
    } else {
      success = false;
    }
  }
  free(jobs);

  if (!success) {
    return EXIT_FAILURE;
  }
  printf("answer: %" PRIu64 "\n", answer);
  return EXIT_SUCCESS;
}
