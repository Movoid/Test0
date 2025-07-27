#include "SimpleCU.h"
#include <bits/stdc++.h>
#include <boost/lockfree/stack.hpp>

#define THREAD_CNT (std::thread::hardware_concurrency())

template<typename ValType>
class TaggedPtr {
private:
  static constexpr uintptr_t PTR_MASK = (1ULL << 48) - 1;
  static constexpr uintptr_t TAG_MASK = ~PTR_MASK;

  uint64_t raw_;

public:
  TaggedPtr() : raw_{} {
  }

  TaggedPtr(ValType *ptr, uint16_t tag) {
    set_ptr(ptr);
    set_tag(tag);
  }

  void set_ptr(ValType *ptr) {
    raw_ = raw_ & TAG_MASK | reinterpret_cast<uint64_t>(ptr);
  }

  void set_tag(uint16_t tag) {
    raw_ = raw_ & PTR_MASK | (static_cast<uint64_t>(tag) << 48);
  }

  ValType *get_ptr() const {
    return reinterpret_cast<ValType *>(raw_ & PTR_MASK);
  }

  uint16_t get_tag() const {
    return static_cast<uint16_t>((raw_ & TAG_MASK) >> 48);
  }
};

template<typename ValType>
class LockFreeStack {
private:
  struct Node;
  struct Node {
    ValType val_;
    std::atomic<int> internal_cnt_; // 读取结束后自减
    TaggedPtr<Node> next_;
    Node(const ValType &val) : val_{val}, internal_cnt_{0} {
    }
  };

  std::atomic<TaggedPtr<Node>> head_;

  void increase_head_count(TaggedPtr<Node> &old_counter) {
    TaggedPtr<Node> new_counter{};
    do {
      new_counter = old_counter;
      new_counter.set_tag(new_counter.get_tag() + 1);
    } while (!head_.compare_exchange_weak(old_counter, new_counter));
    old_counter.set_tag(new_counter.get_tag());
  }

public:
  ~LockFreeStack() {
    while (pop().has_value()) {
      ;
    }
  }

  void push(const ValType &val) {
    TaggedPtr<Node> new_node{new Node{val}, 1};
    do {
      new_node.get_ptr()->next_ = head_.load();
    } while (!head_.compare_exchange_weak(new_node.get_ptr()->next_, new_node));
  }

  std::optional<ValType> pop() {
    TaggedPtr<Node> old_head{head_.load()};
    while (true) {
      increase_head_count(old_head);
      Node *ptr{old_head.get_ptr()};
      if (!ptr) {
        return std::nullopt;
      }
      if (head_.compare_exchange_strong(old_head, ptr->next_)) {
        ValType ret{std::move(ptr->val_)};
        uint16_t count_increase{static_cast<uint16_t>(old_head.get_tag() - 2)};
        if (ptr->internal_cnt_.fetch_add(count_increase) == -count_increase) {
          delete ptr;
        }
        return std::make_optional(std::move(ret));
      } else if (ptr->internal_cnt_.fetch_sub(1) == 1) { // this `old_head` is left
        delete ptr;
      }
    }
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

  // auto beg3{std::chrono::high_resolution_clock::now()};
  // boost_stack_test();
  // auto end3{std::chrono::high_resolution_clock::now()};
  // std::cout << end3 - beg3 << std::endl;

  auto beg1{std::chrono::high_resolution_clock::now()};
  normal_stack_test();
  auto end1{std::chrono::high_resolution_clock::now()};
  std::cout << end1 - beg1 << std::endl;
}