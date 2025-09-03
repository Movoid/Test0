#include "SimpleCU.h"
#include <bits/stdc++.h>
#include <boost/lockfree/stack.hpp>

template<typename ValType>
class LockFreeStack {
private:
  struct Node {
    ValType val_;
    Node *next_;
    Node(const ValType &val) : val_{val} {
    }
  };
  std::atomic<Node *> head_;
  SimpleCU::HazPtrManager<Node, 20, 1> hazptr_manager_;

public:
  void push(const ValType &val) {
    Node *new_node{new Node{val}};
    Node *expected = head_.load(std::memory_order_relaxed);
    new_node->next_ = expected;
    while (!head_.compare_exchange_weak(expected, new_node, std::memory_order_release, std::memory_order_relaxed)) {
      new_node->next_ = expected;
    }
  }

  std::optional<ValType> pop() {
    Node *old_head{head_.load(std::memory_order_relaxed)};
    do {
      Node *tmp{};
      do {
        tmp = old_head;
        hazptr_manager_.set_hazptr(0,
                                   old_head);             // (1) seq_cst store 保证不和 (2) 乱序
        old_head = head_.load(std::memory_order_seq_cst); // (2) seq_cst 必须读到最新值, 来判断 `head_` 这期间没有变化
      } while (old_head != tmp);
      // 如果 `head_` 已经被其他线程 CAS 修改, 还允许 `old_head` 读到旧值, 错误判断 `head_` 没变,
      // `old_head` 可能早在当前线程设置 hazard pointer 之前就被 delete,
      // CAS `old_head->next_` use after free
    } while (old_head && !head_.compare_exchange_weak(old_head, old_head->next_, std::memory_order_seq_cst,
                                                      std::memory_order_relaxed));

    hazptr_manager_.unset_hazptr(0); // release

    if (!old_head) {
      return std::nullopt;
    }

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
    hazptr_manager_.delete_no_hazard_local();
    return std::make_optional(std::move(ret));
  }
};

#define PARA_CNT (20)
#define VALTAG_SCALE 5000000ul

std::size_t PUSH_PARA_CNT{15};
std::size_t POP_PARA_CNT{5};

void lockfree_stack_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  LockFreeStack<int> stack{};

  std::barrier b{PARA_CNT};
  std::vector<std::jthread> js(PARA_CNT);

  for (int i = 0; i < PUSH_PARA_CNT; i++) {
    js[i] = std::jthread{[&stack, &b, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + PUSH_PARA_CNT - 1) / PUSH_PARA_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        stack.push(i);
      }
    }};
  }

  for (int i = 0; i < POP_PARA_CNT; i++) {
    js[PUSH_PARA_CNT + i] = std::jthread{[&valtag, &stack, &b, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + POP_PARA_CNT - 1) / POP_PARA_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end;) {
        std::optional<int> res{stack.pop()};
        if (res.has_value()) {
          i++;
          valtag[res.value()] = 1;
        }
      }
    }};
  }

  for (int i = 0; i < PARA_CNT; i++) {
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
  std::stack<int> stack{};
  std::mutex m{};

  std::barrier b{PARA_CNT};
  std::vector<std::jthread> js(PARA_CNT);

  for (int i = 0; i < PUSH_PARA_CNT; i++) {
    js[i] = std::jthread{[&stack, &b, &m, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + PUSH_PARA_CNT - 1) / PUSH_PARA_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        std::lock_guard<std::mutex> lock{m};
        stack.push(i);
      }
    }};
  }

  for (int i = 0; i < POP_PARA_CNT; i++) {
    js[PUSH_PARA_CNT + i] = std::jthread{[&valtag, &stack, &b, &m, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + POP_PARA_CNT - 1) / POP_PARA_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end;) {
        std::lock_guard<std::mutex> l{m};
        if (stack.empty()) {
          continue;
        }
        i++;
        valtag[stack.top()] = 1;
        stack.pop();
      }
    }};
  }

  for (int i = 0; i < PARA_CNT; i++) {
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
  boost::lockfree::stack<int> stack{VALTAG_SCALE};

  std::barrier b{PARA_CNT};
  std::vector<std::jthread> js(PARA_CNT);

  for (int i = 0; i < PUSH_PARA_CNT; i++) {
    js[i] = std::jthread{[&stack, &b, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + PUSH_PARA_CNT - 1) / PUSH_PARA_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end; i++) {
        stack.push(i);
      }
    }};
  }

  for (int i = 0; i < POP_PARA_CNT; i++) {
    js[PUSH_PARA_CNT + i] = std::jthread{[&valtag, &stack, &b, i]() {
      b.arrive_and_wait();
      std::size_t blksz{(VALTAG_SCALE + POP_PARA_CNT - 1) / POP_PARA_CNT};
      std::size_t beg{blksz * i};
      std::size_t end{std::min(beg + blksz, VALTAG_SCALE)};
      for (int i = beg; i < end;) {
        int res{};
        if (stack.pop(res)) {
          i++;
          valtag[res] = 1;
        }
      }
    }};
  }

  for (int i = 0; i < PARA_CNT; i++) {
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

  auto beg1{std::chrono::high_resolution_clock::now()};
  normal_stack_test();
  auto end1{std::chrono::high_resolution_clock::now()};
  std::cout << end1 - beg1 << std::endl;

  auto beg3{std::chrono::high_resolution_clock::now()};
  boost_stack_test();
  auto end3{std::chrono::high_resolution_clock::now()};
  std::cout << end3 - beg3 << std::endl;
}