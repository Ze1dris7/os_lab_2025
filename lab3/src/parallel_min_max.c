#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            // your code here
            // error handling
            break;
          case 1:
            array_size = atoi(optarg);
            // your code here
            // error handling
            break;
          case 2:
            pnum = atoi(optarg);
            // your code here
            // error handling
            break;
          case 3:
            with_files = true;
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed <= 0 || array_size <= 0 ||
    pnum <= 0 || pnum > array_size) {
  fprintf(stderr,
          "Require seed > 0, array_size > 0, "
          "and 1 <= pnum <= array_size\n");
  return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
  perror("malloc");
  return 1;
  }
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);
  pid_t parent_pid = getpid();
  int (*pipes)[2] = NULL;

  if (!with_files) {
    pipes = malloc((size_t)pnum * sizeof(*pipes));

    if (pipes == NULL) {
      perror("malloc");
      free(array);
      return 1;
    }

    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");

        for (int j = 0; j < i; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }

        free(pipes);
        free(array);
        return 1;
      }
    }
  }



  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // child process

      unsigned int begin =
  (unsigned int)((long long)i * array_size / pnum);

      unsigned int end =
  (unsigned int)((long long)(i + 1) * array_size / pnum);

      struct MinMax local = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[128];

          snprintf(filename, sizeof(filename),
                  "/tmp/lab3_%ld_%d.txt",
                  (long)parent_pid, i);

          FILE *file = fopen(filename, "w");

          if (file == NULL) {
            perror("fopen");
            return 1;
          }

          if (fprintf(file, "%d %d\n", local.min, local.max) < 0) {
            perror("fprintf");
            fclose(file);
            return 1;
          }

          if (fclose(file) == EOF) {
            perror("fclose");
            return 1;
          }
                  } else {
          for (int j = 0; j < pnum; j++) {
            close(pipes[j][0]);

            if (j != i) {
              close(pipes[j][1]);
            }
          }

          FILE *out = fdopen(pipes[i][1], "w");

          if (out == NULL) {
            perror("fdopen");
            close(pipes[i][1]);
            return 1;
          }

          if (fprintf(out, "%d %d\n", local.min, local.max) < 0) {
            perror("fprintf");
            fclose(out);
            return 1;
          }

          if (fclose(out) == EOF) {
            perror("fclose");
            return 1;
          }
        }
        return 0;
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      close(pipes[i][1]);
    }
  }
  bool child_failed = false;

  while (active_child_processes > 0) {
    int status;
    pid_t finished_pid = wait(&status);

    if (finished_pid == -1) {
      if (errno == EINTR) {
        continue;
      }

      perror("wait");
      free(array);
      return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "Child process %ld failed\n",
              (long)finished_pid);
      child_failed = true;
    }

    active_child_processes -= 1;
  }

  if (child_failed) {
    free(array);
    return 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      char filename[128];

      snprintf(filename, sizeof(filename),
              "/tmp/lab3_%ld_%d.txt",
              (long)parent_pid, i);

      FILE *file = fopen(filename, "r");

      if (file == NULL) {
        perror("fopen");
        free(array);
        return 1;
      }

      if (fscanf(file, "%d %d", &min, &max) != 2) {
        fprintf(stderr, "Cannot read result from %s\n", filename);
        fclose(file);
        free(array);
        return 1;
      }

      if (fclose(file) == EOF) {
        perror("fclose");
        free(array);
        return 1;
      }

      if (remove(filename) != 0) {
        perror("remove");
        free(array);
        return 1;
      }
    } else {
      FILE *in = fdopen(pipes[i][0], "r");

      if (in == NULL) {
        perror("fdopen");
        close(pipes[i][0]);
        free(pipes);
        free(array);
        return 1;
      }

      if (fscanf(in, "%d %d", &min, &max) != 2) {
        fprintf(stderr, "Cannot read result from pipe %d\n", i);
        fclose(in);
        free(pipes);
        free(array);
        return 1;
      }

      if (fclose(in) == EOF) {
        perror("fclose");
        free(pipes);
        free(array);
        return 1;
      }
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(pipes);
  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
