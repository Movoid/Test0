#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  printf("sub prog!\n");

  cap_t caps = cap_get_proc();
  cap_flag_value_t cap_val;
  if (cap_get_flag(caps, CAP_SETPCAP, CAP_EFFECTIVE, &cap_val) == -1) {
    perror("cap_get_flag");
  }
  if (cap_val == CAP_SET) {
    printf("CAP_SETPCAP is effective!\n");
  } else {
    printf("CAP_SETPCAP is NOT effective!\n");
  }
  cap_free(caps);

  int rt;

  if ((rt = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_ADMIN, 0, 0)) < 0) {
    perror("prctl");
    _exit(-1);
  }
  // sleep(10);
  printf("finished.\n");
}