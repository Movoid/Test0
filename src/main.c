#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <syscall.h>
#include <unistd.h>

volatile sig_atomic_t flag = 0;

void sigusr1_handler(int sig, siginfo_t *siginfo, void *ucontext) {
  write(STDOUT_FILENO, "1", 2);
  printf("get SIGCONT from %ld.\n", (long)siginfo->si_uid);
}

int main() {

  struct sigaction sigusr1;
  sigemptyset(&sigusr1.sa_mask);
  sigusr1.sa_flags = SA_SIGINFO | SA_RESTART;
  sigusr1.sa_sigaction = sigusr1_handler;
  sigaction(SIGCONT, &sigusr1, NULL);

  sigset_t mask, prev;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCONT);

  pthread_sigmask(SIG_BLOCK, &mask, &prev);

  raise(SIGSTOP);

  pthread_sigmask(SIG_SETMASK, &prev, NULL);
  sleep(3);
  return 0;
}
