#include "SimpleCU_HazPtr.h"
#include <bits/stdc++.h>
using namespace std;

// NOT CONFIRMED BUGFREE
template<typename ValType>
class LockFreeQueue {
private:
  struct Node {
    ValType val_;
    std::atomic<Node *> next_;
    Node() : next_{nullptr} {
    }
  };
  std::atomic<Node *> head_; // sentinel
  std::atomic<Node *> tail_;
  SimpleCU::HazPtr::HazPtrManager<Node, 20, 1> hazptr_manager_;

public:
  LockFreeQueue() : head_{new Node{}}, tail_{head_.load()} {
  }
  LockFreeQueue(const LockFreeQueue &obj) = delete;
  LockFreeQueue &operator=(const LockFreeQueue &obj) = delete;

  ~LockFreeQueue() {
    Node *cur{head_.load(std::memory_order_relaxed)};
    while (cur) {
      Node *next{cur->next_.load(std::memory_order_relaxed)};
      delete cur;
      cur = next;
    }
  }

  std::optional<ValType> pop() {
    Node *old_head{head_.load(std::memory_order_relaxed)};
    Node *old_tail{tail_.load(std::memory_order_relaxed)};
    if (old_head == old_tail) {
      return std::nullopt;
    }
    Node *old_head_next{};
    do {
      Node *tmp{};
      do {
        tmp = old_head;
        hazptr_manager_.set_hazptr(0, old_head);
        old_head = head_.load(std::memory_order_seq_cst);
      } while (old_head != tmp);
      old_head_next = old_head->next_.load();
    } while (old_head_next && !head_.compare_exchange_weak(old_head, old_head_next, std::memory_order_seq_cst,
                                                           std::memory_order_relaxed));
    // acquired `old_head`
    if (!old_head_next) {
      hazptr_manager_.unset_hazptr(0);
      return std::nullopt;
    }
    hazptr_manager_.unset_hazptr(0);

    ValType ret{std::move(old_head->val_)};
    if (hazptr_manager_.get_retired_cnt_local() >= 2 * hazptr_manager_.get_max_hazptr_cnt_global()) {
      if (hazptr_manager_.check_hazptr(old_head)) { // acquire
        hazptr_manager_.retire(old_head);
      } else {
        delete old_head;
      }
    } else {
      hazptr_manager_.retire(old_head);
    }
    hazptr_manager_.reclaim_local();
    return std::make_optional(std::move(ret));
  }

  void push(const ValType &val) {
    Node *old_tail{tail_.load(std::memory_order_relaxed)};
    Node *new_node{new Node{}}; // new empty node
    while (!tail_.compare_exchange_weak(old_tail, new_node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
      ;
    }
    // acquired `old_tail`
    old_tail->val_ = val;
    old_tail->next_.store(new_node, std::memory_order_release);
    return;
  }
};

#define THREAD_CNT (std::thread::hardware_concurrency())
#define VALTAG_SCALE 10000000ul

void lockfree_queue_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  LockFreeQueue<int> queue{};

  std::barrier b{THREAD_CNT};
  std::vector<std::thread> js(THREAD_CNT);

  std::size_t PUSH_THREAD_CNT{THREAD_CNT / 2};
  std::size_t POP_THREAD_CNT{THREAD_CNT - PUSH_THREAD_CNT};

  for (int i = 0; i < PUSH_THREAD_CNT; i++) {
    js[i] = std::thread{[&queue, &b, PUSH_THREAD_CNT, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + PUSH_THREAD_CNT - 1) / PUSH_THREAD_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        queue.push(i);
      }
    }};
  }

  for (int i = 0; i < POP_THREAD_CNT; i++) {
    js[PUSH_THREAD_CNT + i] = std::thread{[&valtag, &queue, &b, POP_THREAD_CNT, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + POP_THREAD_CNT - 1) / POP_THREAD_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end;) {
        std::optional<int> res{queue.pop()};
        if (res.has_value()) {
          i++;
          valtag[res.value()] = 1;
        }
      }
    }};
  }

  for (int i = 0; i < THREAD_CNT; i++) {
    js[i].join();
  }

  bool passed{true};
  int checksum{};
  for (int i = 0; i < VALTAG_SCALE; i++) {
    checksum += valtag[i];
    if (valtag[i] != 1) {
      passed = false;
      break;
    }
  }
  std::cout << (passed ? "passed" : "failed") << std::endl;
  std::cout << checksum << std::endl;
}

void normal_queue_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  // LockFreeQueue<int> queue{};
  std::queue<int> queue{};
  std::mutex m{};

  std::barrier b{THREAD_CNT};
  std::vector<std::thread> js(THREAD_CNT);

  std::size_t PUSH_THREAD_CNT{THREAD_CNT / 2};
  std::size_t POP_THREAD_CNT{THREAD_CNT - PUSH_THREAD_CNT};

  for (int i = 0; i < PUSH_THREAD_CNT; i++) {
    js[i] = std::thread{[&queue, &b, &m, PUSH_THREAD_CNT, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + PUSH_THREAD_CNT - 1) / PUSH_THREAD_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        std::lock_guard<std::mutex> lock{m};
        queue.push(i);
      }
    }};
  }

  for (int i = 0; i < POP_THREAD_CNT; i++) {
    js[PUSH_THREAD_CNT + i] = std::thread{[&valtag, &queue, &b, &m, POP_THREAD_CNT, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + POP_THREAD_CNT - 1) / POP_THREAD_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end;) {
        std::lock_guard<std::mutex> l{m};
        if (queue.empty()) {
          continue;
        }
        i++;
        valtag[queue.front()] = 1;
        queue.pop();
      }
    }};
  }

  for (int i = 0; i < THREAD_CNT; i++) {
    js[i].join();
  }

  bool passed{true};
  int checksum{};
  for (int i = 0; i < VALTAG_SCALE; i++) {
    checksum += valtag[i];
    if (valtag[i] != 1) {
      passed = false;
      break;
    }
  }
  std::cout << (passed ? "passed" : "failed") << std::endl;
  std::cout << checksum << std::endl;
}

int main() {

  auto beg1{std::chrono::high_resolution_clock::now()};
  lockfree_queue_test();
  auto end1{std::chrono::high_resolution_clock::now()};
  std::cout << end1 - beg1 << std::endl;

  auto beg2{std::chrono::high_resolution_clock::now()};
  normal_queue_test();
  auto end2{std::chrono::high_resolution_clock::now()};
  std::cout << end2 - beg2 << std::endl;
}