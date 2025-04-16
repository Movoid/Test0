#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include <sys/time.h>

void syscall_puts(char *str) {
  write(STDOUT_FILENO, str, strlen(str));
}

void set_signal(int sig, void (*handler)(int, siginfo_t *, void *)) {
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigaction(sig, &sa, NULL);
}

#define HANDLER_ARGS int sig, siginfo_t *siginfo, void *ucontext

void sigalrm_handler(HANDLER_ARGS) {

  syscall_puts("sigalrm!\n");
}

void sigcont_handler(HANDLER_ARGS) {
  syscall_puts("sigcont!\n");
}

void sigusr1_handler(HANDLER_ARGS) {
  syscall_puts("sigusr1!\n");
  printf("getoverrun %d.\n", timer_getoverrun(*(timer_t *)siginfo->si_value.sival_ptr));
}

void block(int sig) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, sig);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void unblock(int sig) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, sig);
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
}

void new_thread_start(sigval_t sigval) {
  printf("new thread! tid %d \n", gettid());
}

void exit_call() {
  puts("bye!!\n");
}

int main() {

  atexit(exit_call);

  set_signal(SIGALRM, sigalrm_handler);
  set_signal(SIGCONT, sigcont_handler);
  set_signal(SIGUSR1, sigusr1_handler);

  printf("tid %d \n", gettid());

  // create a timer using SIGUSR1 to alarm.
  timer_t timerid;
  sigevent_t se = {.sigev_notify = SIGEV_SIGNAL,
                   .sigev_signo = SIGUSR1,
                   .sigev_value.sival_ptr = &timerid,
                   .sigev_notify_function = new_thread_start,
                   .sigev_notify_attributes = NULL};
  timer_create(CLOCK_REALTIME, &se, &timerid);

  // set timer and start
  struct itimerspec timeout = {.it_value.tv_sec = 3, .it_interval = {1, 0}};
  timer_settime(timerid, 0, &timeout, NULL);

  // wait for SIGUSR1.
  block(SIGUSR1);
  sleep(5);
  unblock(SIGUSR1);

  return 0;
}