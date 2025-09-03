#include <bits/stdc++.h>
using namespace std;

void enter();
void exit();
void retire();
void try_advance_and_collect();
void delete_node(void *);

struct ThreadRec {
  std::atomic<uint64_t> announced{UINT64_MAX}; // UINT64_MAX == INACTIVE
  std::vector<void *> bags[3];                 // Limbo list
  int bag_sizes[3]{};
  int retire_count{};
};

std::vector<ThreadRec *> all_threads{};
thread_local ThreadRec *me;
std::atomic<uint64_t> global_epoch{0};

void enter() {
  uint64_t e = global_epoch.load(std::memory_order_acquire);
  me->announced.store(e, std::memory_order_release);
}

void exit() {
  me->announced.store(UINT64_MAX, std::memory_order_release);
}

/**
 * `retire` 必须在 `exit` 之前调用,
 * 以避免当前线程在 `try_advance_and_collect` 中,
 * 另一个线程也发生一次 `global_epoch` 推进.
 */
void retire(void *p) {
  uint64_t e = global_epoch.load(std::memory_order_relaxed);
  int idx = e % 3;
  me->bags[idx].push_back(p);
  if (++me->retire_count % 64 == 0) try_advance_and_collect(); // 批处理门限
}

void try_advance_and_collect() {
  uint64_t g_epoch = global_epoch.load(std::memory_order_acquire);

  // 检查所有线程, 必须 `announced == e` 或 `INACTIVE`
  for (ThreadRec *t : all_threads) {
    uint64_t t_epoch = t->announced.load(std::memory_order_acquire);
    if (t_epoch != g_epoch && t_epoch != UINT64_MAX) return; // 仍有人停在老纪元
  }

  // 尝试推进纪元
  uint64_t new_epoch{(g_epoch + 1) % 3};
  if (!global_epoch.compare_exchange_strong(g_epoch, new_epoch, std::memory_order_acq_rel)) return;

  // 回收所有的 Limbo list
  for (ThreadRec *t : all_threads) {
    for (void *p : t->bags[new_epoch]) {
      delete_node(p);
    }
    t->bags[new_epoch].clear();
  }
}

int main() {
}