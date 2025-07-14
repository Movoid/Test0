#include <atomic>
#include <barrier>
#include <iostream>
#include <thread>
using namespace std;

std::atomic<bool> flag[2]{};
int sum{};
std::atomic<int> turn{};

std::barrier b1{2}; // start from the same time point

void p0() {
  b1.arrive_and_wait();
  flag[0].store(true, std::memory_order_relaxed);
  turn.store(1, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_seq_cst); // 只能 fence
  while (flag[1].load(std::memory_order_relaxed) && turn.load(std::memory_order_relaxed) == 1) {
    ;
  }
  for (int i = 0; i < 10000; i++) {
    sum++;
  }
  flag[0].store(false, std::memory_order_release);
}

void p1() {
  b1.arrive_and_wait();
  flag[1].store(true, std::memory_order_relaxed);
  turn.store(0, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  while (flag[0].load(std::memory_order_relaxed) && turn.load(std::memory_order_relaxed) == 0) {
    ;
  }
  for (int i = 0; i < 10000; i++) {
    sum++;
  }
  flag[1].store(false, std::memory_order_release);
}

int main() {

  for (int i = 0; i < 10000; i++) {
    std::thread j1{p0};
    std::thread j2{p1};
    j1.join();
    j2.join();
  }
  cout << sum << endl;
}
