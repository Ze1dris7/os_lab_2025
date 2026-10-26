#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s seed arraysize\n", argv[0]);
    return 1;
  }

  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    execl("./sequential_min_max",
          "sequential_min_max",
          argv[1],
          argv[2],
          (char *)NULL);

    perror("execl");
    _exit(127);
  }

  int status;

  while (waitpid(pid, &status, 0) == -1) {
    if (errno == EINTR) {
      continue;
    }

    perror("waitpid");
    return 1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  return 1;
}