#include <bits/stdc++.h>
using namespace std;

namespace MCS {
  enum class MCSFlag {
    WAITING = 0,
    GRANTED = 1,
  };

  struct alignas(std::hardware_constructive_interference_size) MCSNode {
    std::atomic<MCSFlag> flag_{MCSFlag::WAITING}; // WAITING / GRANTED
    std::atomic<MCSNode *> next_{nullptr};
  };

  // if is nullptr then there is no waiting thread.
  struct MCSLock {
    std::atomic<MCSNode *> tail_{nullptr};
  };

  void lock(MCSLock *lock, MCSNode *me) {
    me->next_.store(nullptr, std::memory_order_seq_cst);
    me->flag_.store(MCSFlag::WAITING, std::memory_order_seq_cst);
    MCSNode *pred{lock->tail_.exchange(me, std::memory_order_seq_cst)};
    // (A): currently the tail is not modified to point to `me`.
    // there has been one thread owning the lock.
    if (pred) {
      pred->next_.store(me, std::memory_order_seq_cst);
      while (me->flag_.load(std::memory_order_seq_cst) != MCSFlag::GRANTED)
        ;
    }
  }

  void unlock(MCSLock *lock, MCSNode *me) {
    // no one waiting.
    if (!me->next_.load(std::memory_order_seq_cst)) {
      // change lock to nullptr.
      MCSNode *expected{me};
      if (lock->tail_.compare_exchange_strong(expected, nullptr, std::memory_order_seq_cst,
                                              std::memory_order_seq_cst)) {
        return;
      }
      // (A'): if here is reached, then some another thread has reached A.
      // avoiding race, waiting for the new thread who called `lock()`.
      while (!me->next_.load(std::memory_order_seq_cst))
        ;
    }
    // move lock ownership to next thread.
    me->next_.load(std::memory_order_seq_cst)->flag_.store(MCSFlag::GRANTED, std::memory_order_seq_cst);
  }

} // namespace MCS

int main() {

  constexpr int THREADCNT{8};
  long long var{};
  MCS::MCSLock lock{};
  std::barrier bar{THREADCNT};

  std::vector<std::jthread> js(THREADCNT);
  std::vector<MCS::MCSNode> nodes(THREADCNT);

  for (int i = 0; i < THREADCNT; i++) {
    js[i] = std::jthread{[&var, &bar, &lock, &nodes, i]() {
      bar.arrive_and_wait();
      MCS::lock(&lock, &nodes[i]);
      for (int i = 0; i < 100; i++) {
        var++;
      }
      MCS::unlock(&lock, &nodes[i]);
    }};
  }
  for (int i = 0; i < THREADCNT; i++) {
    js[i].join();
  }

  cout << var << endl;
  return 0;
}
