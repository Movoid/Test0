#include "limits.h"
// #include "sys/reboot.h"
#include "linux/reboot.h"
#include "sys/syscall.h"
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <linux/futex.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

using namespace std;

#define THREADS 20
#define LOOP 1000000
#define ROUNDS 10

volatile int sum = 0;
pthread_barrier_t barrier;

// ------------------------ My Futex Mutex ------------------------ //

// unsigned char atomic_bit_test_set(long *addr, long bit) {
//   unsigned char out;
//   asm volatile("lock btsq %[bit], %[addr]\n\t"
//                "setc %[out]"
//                : [addr] "+m"(*addr), [out] "=q"(out)
//                : [bit] "Ir"(bit)
//                : "memory", "cc");
//   return out;
// }

// long atomic_increase(long *addr) {
//   long val = 1;
//   asm volatile("lock xaddq %[val], %[addr]"
//                : [val] "+r"(val), [addr] "+m"(*addr)
//                :
//                : "cc", "memory");
//   return val - 1;
// }

// long atomic_decrease(long *addr) {
//   long val = -1;
//   asm volatile("lock xaddq %[val], %[addr]"
//                : [val] "+r"(val), [addr] "+m"(*addr)
//                :
//                : "cc", "memory");
//   return val + 1;
// }

// unsigned char atomic_addzero(long *addr, long val) {
//   unsigned char res;
//   asm volatile("lock addq %[val], %[addr]\n\t"
//                "sete %[res]"
//                : [addr] "+m"(*addr), [res] "=q"(res)
//                : [val] "ir"(val)
//                : "cc", "memory");
//   return res;
// }

// void futex_mutex_lock(long *mutex) {
//   if (atomic_bit_test_set(mutex, 63) == 0) return;
//   atomic_increase(mutex);
//   while (true) {
//     if (atomic_bit_test_set(mutex, 63) == 0) {
//       atomic_decrease(mutex);
//       return;
//     }
//     long val = *mutex;
//     if (val >= 0) continue;
//     syscall(SYS_futex, mutex, FUTEX_WAIT, val, NULL, NULL, 0);
//   }
// }

// void futex_mutex_unlock(long *mutex) {
//   if (atomic_addzero(mutex, 0x8000000000000000)) return;
//   syscall(SYS_futex, mutex, FUTEX_WAKE, 1, NULL, NULL, 0);
// }

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int done1{};

void *work1(void *ptr) {
  sleep(2);
  pthread_mutex_lock(&mutex);
  done1 = 1;
  pthread_mutex_unlock(&mutex);
  printf("%d\n", 1);
  pthread_cond_signal(&cond);
  return nullptr;
}

void *work2(void *ptr) {
  pthread_mutex_lock(&mutex);
  while (done1 != 1) {
    pthread_cond_wait(&cond, &mutex);
  }

  printf("%d\n", 2);
  return nullptr;
}

typedef struct aaa_t {
  int a;
  int b;
} aaa_t;

int main() {

  char *hello = (char *)malloc(sizeof(char) * 2);
  hello[0] = 0xff;

  FILE *f = fopen("./111", "w+");
  fwrite(hello, sizeof(char), strlen(hello), f);

  fclose(f);

  return 0;
}