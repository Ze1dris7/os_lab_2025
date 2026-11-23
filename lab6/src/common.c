#include "common.h"

#include <errno.h>
#include <inttypes.h>
#include <sys/socket.h>

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  return (uint64_t)((__uint128_t)a * b % mod);
}

bool ParseUInt64(const char *text, uint64_t *value) {
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

bool SendAll(int fd, const void *buffer, size_t size) {
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

int RecvAll(int fd, void *buffer, size_t size) {
  char *bytes = buffer;
  size_t offset = 0;
  while (offset < size) {
    ssize_t received = recv(fd, bytes + offset, size - offset, 0);
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received < 0) {
      return -1;
    }
    if (received == 0) {
      if (offset == 0) {
        return 0;
      }
      errno = ECONNRESET;
      return -1;
    }
    offset += (size_t)received;
  }
  return 1;
}
