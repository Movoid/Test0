#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

#define WATCH_PATH "./a.txt"
#define BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

void print_event(struct inotify_event *e) {
  printf("[EVENT] ");

  if (e->mask & IN_ACCESS) printf("IN_ACCESS ");
  if (e->mask & IN_ATTRIB) printf("IN_ATTRIB ");
  if (e->mask & IN_CLOSE_WRITE) printf("IN_CLOSE_WRITE ");
  if (e->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
  if (e->mask & IN_CREATE) printf("IN_CREATE ");
  if (e->mask & IN_DELETE) printf("IN_DELETE ");
  if (e->mask & IN_DELETE_SELF) printf("IN_DELETE_SELF ");
  if (e->mask & IN_MODIFY) printf("IN_MODIFY ");
  if (e->mask & IN_MOVE_SELF) printf("IN_MOVE_SELF ");
  if (e->mask & IN_OPEN) printf("IN_OPEN ");

  if (e->mask & IN_IGNORED) printf("IN_IGNORED ");
  if (e->mask & IN_UNMOUNT) printf("IN_UNMOUNT ");
  if (e->mask & IN_Q_OVERFLOW) printf("IN_Q_OVERFLOW ");
  if (e->mask & IN_ISDIR) printf("IN_ISDIR ");

  printf("\n");
}

int main() {

  int fd = inotify_init1(IN_NONBLOCK);
  if (fd == -1) {
    perror("inotify_init1");
    _exit(EXIT_FAILURE);
  }

  int wd = inotify_add_watch(fd, WATCH_PATH, IN_ALL_EVENTS);
  if (wd == -1) {
    perror("inotify_add_watch");
    _exit(EXIT_FAILURE);
  }

  printf("Watching %s...\n", WATCH_PATH);

  // rename the watch file
  // will generate `IN_MOVE_SELF` .
  rename("./a.txt", "./b.txt");

  char buf[BUF_LEN];
  while (1) {
    int len = read(fd, buf, BUF_LEN);
    if (len == -1) {
      if (errno == EAGAIN) {
        usleep(100000); // wait 100ms
        continue;
      } else {
        perror("read");
        break;
      }
    }

    for (char *ptr = buf; ptr < buf + len;) {
      struct inotify_event *e = (struct inotify_event *)ptr;
      print_event(e);
      ptr += sizeof(struct inotify_event) + e->len;
    }
  }

  inotify_rm_watch(fd, wd);

  close(fd);
  return 0;
}
