#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
using namespace std;

constexpr int MAX_EVENTS = 5;

int main(int argc, char *argv[]) {

  int epfd{}, ready_cnt{}, fd{}, s{}, j{}, num_open_fds{};
  struct epoll_event ev{};
  struct epoll_event evlist[MAX_EVENTS]{};

  char buf[BUFSIZ]{};

  epfd = epoll_create1(EPOLL_CLOEXEC);

  for (int j = 1; j < argc; j++) {
    if ((fd = open(argv[j], O_RDONLY)) == -1) {
      perror("open");
    }
    printf("opened \"%s\" on fd %d\n", argv[j], fd);
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
      perror("epoll_ctl");
    }
  }

  num_open_fds = argc - 1;

  while (num_open_fds > 0) {
    printf("About to epoll_wait\n");
    if ((ready_cnt = epoll_wait(epfd, evlist, MAX_EVENTS, -1)) == -1) {
      if (errno == EINTR) {
        continue;
      } else {
        perror("epoll_wait");
      }
    }
    printf("ready cnt: %d.\n", ready_cnt);
    for (j = 0; j < ready_cnt; j++) {
      printf("\tfd=%dp; events:%s%s%s\n", evlist[j].data.fd, (evlist[j].events & EPOLLIN) ? "EPOLLIN" : "",
             (evlist[j].events & EPOLLHUP) ? "EPOLLHUP" : "", (evlist[j].events & EPOLLERR) ? "EPOLLERR" : "");
      if (evlist[j].events & EPOLLIN) {
        if ((s = read(evlist[j].data.fd, buf, sizeof(buf))) == -1) {
          perror("read");
        }
      } else if (evlist[j].events & (EPOLLHUP | EPOLLERR)) {
        printf("\tclosing fd %d.\n", evlist[j].data.fd);
      }
      if (close(evlist[j].data.fd) == -1) {
        perror("close");
      }
      num_open_fds--;
    }
  }
  puts("bye");
}