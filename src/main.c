#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// const char FIFO_WR_NAME[] = "./fifo_wr";
// const char FIFO_RD_NAME[] = "./fifo_rd";

void fatal(const char s[]) {
  char buf[BUFSIZ];
  strerror_r(errno, buf, sizeof(buf));
  buf[BUFSIZ - 1] = '\0';
  fprintf(stderr, "%s: A fatal error occurred. %s\n", s, buf);
  fprintf(stderr, "Process exit...\n");
  _exit(1);
}

int main(int argc, char *argv[]) {

  char *FIFO_OUT = argv[1];
  char *FIFO_IN = argv[2];

  if (argc != 3) {
    fatal("main");
  }

  int rt;

  struct stat st;

  if (access(FIFO_OUT, W_OK) == -1) {
    puts("remove existed fifo_rw...");
    unlink(FIFO_OUT);
    if ((rt = mkfifo(FIFO_OUT, 0644)) < 0) {
      fatal("mkfifo");
    }
  }

  if (access(FIFO_IN, R_OK) == -1) {
    puts("remove existed fifo_rd...");
    unlink(FIFO_IN);
    if ((rt = mkfifo(FIFO_IN, 0644)) < 0) {
      fatal("mkfifo");
    }
  }

  int fifo_wr;
  int fifo_rd = open(FIFO_IN, O_RDONLY | O_NONBLOCK);
  int timeout = 0;
  struct timespec wt, rem;
  wt.tv_nsec = 0;
  wt.tv_sec = 1;
  while ((fifo_wr = open(FIFO_OUT, O_WRONLY | O_NONBLOCK)) == -1) {
    if (timeout == 10) {
      fatal("open");
    }
    puts("waiting fifo reader...");
    if (errno == ENXIO) {
      clock_nanosleep(CLOCK_REALTIME, 0, &wt, &rem);
    } else {
      fatal("open");
    }
    timeout++;
  }
  puts("fifo available.");

  int fifo_wr_flags = fcntl(fifo_wr, F_GETFL);
  fifo_wr_flags &= ~O_NONBLOCK;
  fcntl(fifo_wr, F_SETFL, fifo_wr_flags);

  int fifo_rd_flags = fcntl(fifo_rd, F_GETFL);
  fifo_rd_flags &= ~O_NONBLOCK;
  fcntl(fifo_rd, F_SETFL, fifo_rd_flags);

  int cnt = 0;
  char buf[BUFSIZ];
  while (cnt++ < 10) {
    snprintf(buf, sizeof(buf), "message%d", cnt);
    write(fifo_wr, buf, strlen(buf) + 1);
    while ((rt = read(fifo_rd, buf, sizeof(buf))) <= 0) {
      if (errno != EAGAIN && rt == -1) {
        fatal("read");
      }
    }
    printf("received echo: %s\n", buf);
    clock_nanosleep(CLOCK_REALTIME, 0, &wt, &rem);
  }

  close(fifo_wr);

  unlink(FIFO_IN);
  unlink(FIFO_OUT);
  puts("finished.");

  return 0;
}