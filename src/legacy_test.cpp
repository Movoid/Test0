// #include <bits/stdc++.h>
#include <atomic>
#include <barrier>
#include <boost/lockfree/stack.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#define THREAD_CNT (std::thread::hardware_concurrency())

const unsigned max_hazard_pointers{100};
struct hazard_pointer {
  std::atomic<std::thread::id> id;
  std::atomic<void *> pointer;
};

// ptr pool for all threads
hazard_pointer hazard_ptrs[max_hazard_pointers]{};

class hp_owner {
private:
  hazard_pointer *hp;

public:
  hp_owner(const hp_owner &) = delete;
  hp_owner operator=(const hp_owner &) = delete;
  hp_owner() : hp{nullptr} {
    for (unsigned int i = 0; i < max_hazard_pointers; i++) {
      std::thread::id old_id; // empty id
      if (hazard_ptrs[i].id.compare_exchange_strong(old_id, std::this_thread::get_id(), std::memory_order_acquire,
                                                    std::memory_order_relaxed)) {
        hp = &hazard_ptrs[i];
        break;
      }
    }
    // not found
    if (!hp) {
      throw std::runtime_error{"No hazard ptr available"};
    }
  }
  std::atomic<void *> &get_pointer() {
    return hp->pointer;
  }
  ~hp_owner() {
    hp->pointer.store(nullptr, std::memory_order_relaxed);
    hp->id.store(std::thread::id{}, std::memory_order_release);
  }
};

std::atomic<void *> &get_hazard_pointer_for_current_thread() {
  thread_local static hp_owner hazard;
  return hazard.get_pointer();
}

template<typename T>
void do_delete(void *p) {
  delete static_cast<T *>(p);
}

struct data_to_reclaim {
  void *data_;
  std::function<void(void *)> deleter_;
  data_to_reclaim *next_;

  template<typename T>
  data_to_reclaim(T *p) : data_{p}, deleter_{&do_delete<T>}, next_{nullptr} {
  }
  ~data_to_reclaim() {
    deleter_(data_);
  }
};

std::atomic<data_to_reclaim *> nodes_to_reclaim{};
std::atomic<int> nodes_to_reclaim_cnt{};

void add_to_reclaim_list(data_to_reclaim *node) {
  node->next_ = nodes_to_reclaim.load();
  while (!nodes_to_reclaim.compare_exchange_weak(node->next_, node, std::memory_order_release,
                                                 std::memory_order_relaxed)) {
    ;
  }
  nodes_to_reclaim_cnt.fetch_add(1, std::memory_order_relaxed);
}

template<typename T>
void reclaim_later(T *data) {
  add_to_reclaim_list(new data_to_reclaim{data});
}

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

  bool outstanding_hazard_ptrs_for(Node *node) {
    for (unsigned i = 0; i < max_hazard_pointers; i++) {
      if (hazard_ptrs[i].pointer.load(std::memory_order_acquire) == node) {
        return true;
      }
    }
    return false;
  }

  void delete_nodes_with_no_hazards() {
    // data_to_reclaim *current{nodes_to_reclaim.exchange(nullptr)};

    int reclaim_cnt{};
    data_to_reclaim *current{nodes_to_reclaim.load(std::memory_order_acquire)};
    do {
      reclaim_cnt = nodes_to_reclaim_cnt.load(std::memory_order_relaxed);
    } while (!nodes_to_reclaim.compare_exchange_weak(current, nullptr, std::memory_order_acq_rel,
                                                     std::memory_order_relaxed));
    nodes_to_reclaim_cnt.fetch_sub(reclaim_cnt, std::memory_order_relaxed);

    while (current) {
      data_to_reclaim *next{current->next_};
      if (!outstanding_hazard_ptrs_for(reinterpret_cast<Node *>(current->data_))) {
        delete current;
      } else {
        add_to_reclaim_list(current); // 不可回收
      }
      current = next;
    }
  }

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
    std::atomic<void *> &hazardptr{get_hazard_pointer_for_current_thread()};
    Node *old_head{head_.load(std::memory_order_relaxed)};
    do {
      Node *tmp{};
      do {
        tmp = old_head;
        hazardptr.store(old_head, std::memory_order_seq_cst); // (1)
        old_head = head_.load(std::memory_order_seq_cst);     // (2) this shouldn't be modified
      } while (old_head != tmp);

    } while (old_head && !head_.compare_exchange_weak(old_head, old_head->next_, std::memory_order_seq_cst,
                                                      std::memory_order_relaxed));

    hazardptr.store(nullptr, std::memory_order_release);

    if (!old_head) {
      return std::nullopt;
    }

    ValType ret{std::move(old_head->val_)};
    if (nodes_to_reclaim_cnt.load(std::memory_order_relaxed) >= 2 * max_hazard_pointers) {
      if (outstanding_hazard_ptrs_for(old_head)) {
        reclaim_later(old_head);
      } else {
        delete old_head;
      }
    } else {
      reclaim_later(old_head);
    }
    delete_nodes_with_no_hazards();
    return std::make_optional(std::move(ret));
  }
};

#define VALTAG_SCALE 5000000u

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