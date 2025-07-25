#include "SimpleCU.h"
#include <bits/stdc++.h>
#include <boost/lockfree/stack.hpp>

#define THREAD_CNT (std::thread::hardware_concurrency())

template<typename ValType>
class LockFreeStack {
private:
  struct Node {
    ValType val_;
    // Node *next_;
    std::atomic<Node *> next_;
    Node(const ValType &val) : val_{val} {
    }
  };
  std::atomic<Node *> head_;
  SimpleCU::HazPtrManager<Node, 20, 1> hazptr_manager_;

public:
  void push(const ValType &val) {
    Node *new_node{new Node{val}};
    Node *expected = head_.load(std::memory_order_seq_cst);
    new_node->next_ = expected;
    while (!head_.compare_exchange_weak(expected, new_node, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
      new_node->next_ = expected;
    }
  }

  std::optional<ValType> pop() {
    Node *old_head{head_.load(std::memory_order_seq_cst)};
    do {
      Node *tmp{};
      do {
        tmp = old_head;
        hazptr_manager_.set_hazptr(0, old_head);
        old_head = head_.load(std::memory_order_seq_cst); // (2) this shouldn't be modified
      } while (old_head != tmp);
    } while (old_head && !head_.compare_exchange_weak(old_head, old_head->next_, std::memory_order_seq_cst, // ?
                                                      std::memory_order_seq_cst));

    hazptr_manager_.set_hazptr(0, nullptr);

    if (!old_head) {
      return std::nullopt;
    }

    ValType ret{std::move(old_head->val_)};
    if (hazptr_manager_.get_retired_cnt_local() >= 2 * hazptr_manager_.get_max_hazptr_cnt_global()) {
      if (hazptr_manager_.check_hazptr(old_head)) {
        hazptr_manager_.retire(old_head);
      } else {
        delete old_head;
      }
    } else {
      hazptr_manager_.retire(old_head);
    }
    hazptr_manager_.delete_no_hazard_local();
    return std::make_optional(std::move(ret));
  }
};

#define VALTAG_SCALE 1000000u

void lockfree_stack_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  LockFreeStack<int> stack{};

  std::barrier b{THREAD_CNT};
  std::vector<std::thread> js(THREAD_CNT);

  for (int i = 0; i < THREAD_CNT; i++) {
    js[i] = std::thread{[&valtag, &stack, &b, i]() {
      b.arrive_and_wait();
      unsigned int blksz{(VALTAG_SCALE + THREAD_CNT - 1) / THREAD_CNT};
      unsigned int beg{blksz * i};
      unsigned int end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        stack.push(i);
      }
      b.arrive_and_wait();
      std::optional<int> res{};
      while ((res = stack.pop()).has_value()) {
        valtag[res.value()] = 1;
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

void normal_stack_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  std::vector<int> stack(VALTAG_SCALE, 0);
  std::mutex m{};

  std::barrier b{THREAD_CNT};
  std::vector<std::thread> js(THREAD_CNT);

  for (int i = 0; i < THREAD_CNT; i++) {
    js[i] = std::thread{[&valtag, &stack, &m, &b, i]() {
      b.arrive_and_wait();
      unsigned int blksz{(VALTAG_SCALE + THREAD_CNT - 1) / THREAD_CNT};
      unsigned int beg{blksz * i};
      unsigned int end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        std::lock_guard<std::mutex> l{m};
        stack.push_back(i);
      }
      b.arrive_and_wait();
      std::optional<int> res{};
      while (true) {
        std::lock_guard<std::mutex> l{m};
        if (stack.empty()) {
          break;
        }
        valtag[stack.back()] = 1;
        stack.pop_back();
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

void boost_stack_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  // std::vector<int> stack(VALTAG_SCALE, 0);
  boost::lockfree::stack<int> stack{VALTAG_SCALE};

  std::barrier b{THREAD_CNT};
  std::vector<std::thread> js(THREAD_CNT);

  for (int i = 0; i < THREAD_CNT; i++) {
    js[i] = std::thread{[&valtag, &stack, &b, i]() {
      b.arrive_and_wait();
      unsigned int blksz{(VALTAG_SCALE + THREAD_CNT - 1) / THREAD_CNT};
      unsigned int beg{blksz * i};
      unsigned int end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        // stack.push_back(i);
        stack.push(i);
      }
      b.arrive_and_wait();
      // std::optional<int> res{};
      // while ((res = stack.pop()).has_value()) {
      //   valtag[res.value()] = 1;
      // }
      int res{};
      while (stack.pop(res)) {
        valtag[res] = 1;
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

  auto beg2{std::chrono::high_resolution_clock::now()};
  lockfree_stack_test();
  auto end2{std::chrono::high_resolution_clock::now()};
  std::cout << end2 - beg2 << std::endl;

  auto beg3{std::chrono::high_resolution_clock::now()};
  boost_stack_test();
  auto end3{std::chrono::high_resolution_clock::now()};
  std::cout << end3 - beg3 << std::endl;

  auto beg1{std::chrono::high_resolution_clock::now()};
  normal_stack_test();
  auto end1{std::chrono::high_resolution_clock::now()};
  std::cout << end1 - beg1 << std::endl;
}