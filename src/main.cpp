#include <atomic>
#include <chrono>
#include <linux/futex.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define THREADS 20
#define LOOP 1000000
#define ROUNDS 10

volatile int sum = 0;
pthread_barrier_t barrier;

// ------------------------ My Futex Mutex ------------------------ //

unsigned char atomic_bit_test_set(long *addr, long bit) {
  unsigned char out;
  asm volatile("lock btsq %[bit], %[addr]\n\t"
               "setc %[out]"
               : [addr] "+m"(*addr), [out] "=q"(out)
               : [bit] "Ir"(bit)
               : "memory", "cc");
  return out;
}

long atomic_increase(long *addr) {
  long val = 1;
  asm volatile("lock xaddq %[val], %[addr]"
               : [val] "+r"(val), [addr] "+m"(*addr)
               :
               : "cc", "memory");
  return val - 1;
}

long atomic_decrease(long *addr) {
  long val = -1;
  asm volatile("lock xaddq %[val], %[addr]"
               : [val] "+r"(val), [addr] "+m"(*addr)
               :
               : "cc", "memory");
  return val + 1;
}

unsigned char atomic_addzero(long *addr, long val) {
  unsigned char res;
  asm volatile("lock addq %[val], %[addr]\n\t"
               "sete %[res]"
               : [addr] "+m"(*addr), [res] "=q"(res)
               : [val] "ir"(val)
               : "cc", "memory");
  return res;
}

void futex_mutex_lock(long *mutex) {
  if (atomic_bit_test_set(mutex, 63) == 0) return;
  atomic_increase(mutex);
  while (true) {
    if (atomic_bit_test_set(mutex, 63) == 0) {
      atomic_decrease(mutex);
      return;
    }
    long val = *mutex;
    if (val >= 0) continue;
    syscall(SYS_futex, mutex, FUTEX_WAIT, val, NULL, NULL, 0);
  }
}

void futex_mutex_unlock(long *mutex) {
  if (atomic_addzero(mutex, 0x8000000000000000)) return;
  syscall(SYS_futex, mutex, FUTEX_WAKE, 1, NULL, NULL, 0);
}

class lazy_counter_t {
private:
  int global_{};
  pthread_mutex_t glock_{};
  int local_[20]{};
  pthread_mutex_t llock_[20]{};
  int threshold_{};

public:
  lazy_counter_t(int threshold) : threshold_(threshold), global_(0) {
    pthread_mutex_init(&glock_, nullptr);
    for (int i = 0; i < 20; i++) {
      local_[i] = 0;
      pthread_mutex_init(&llock_[i], nullptr);
    }
  }

  void update(int threadID, int amt) {
    pthread_mutex_lock(&llock_[threadID]);
    local_[threadID] += amt;
    if (local_[threadID] >= threshold_) {
      pthread_mutex_lock(&glock_);
      global_ += local_[threadID];
      pthread_mutex_unlock(&glock_);
      local_[threadID] = 0;
    }
    pthread_mutex_unlock(&llock_[threadID]);
  }

  int get() {
    pthread_mutex_lock(&glock_);
    int val{global_};
    pthread_mutex_unlock(&glock_);
    return val;
  }
  // howiqfhbwqif
};

int main() {
}