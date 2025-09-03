#include <barrier>
#include <bits/stdc++.h>
#include <boost/lockfree/stack.hpp>

#define PARA_CNT (std::thread::hardware_concurrency())

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
    std::atomic<int> in_refcnt;
    TaggedPtr<Node> next_;
    Node(const ValType &val) : val_{val}, in_refcnt{0} {
    }
  };

  std::atomic<TaggedPtr<Node>> head_;

  void increase_headptr_refcnt(TaggedPtr<Node> &old_head) {
    TaggedPtr<Node> updated_head{};
    do {
      updated_head = old_head;
      updated_head.set_tag(updated_head.get_tag() + 1);
    } while (
        !head_.compare_exchange_weak(old_head, updated_head, std::memory_order_acquire, std::memory_order_relaxed));
    old_head.set_tag(updated_head.get_tag());
  }

public:
  ~LockFreeStack() {
    Node *cur{head_.load().get_ptr()};
    while (cur) {
      Node *next{cur->next_.get_ptr()};
      delete cur;
      cur = next;
    }
  }

  void push(const ValType &val) {
    TaggedPtr<Node> new_head{new Node{val}, 1};
    new_head.get_ptr()->next_ = head_.load();
    while (!head_.compare_exchange_weak(
        new_head.get_ptr()->next_, new_head, std::memory_order_release,
        std::memory_order_relaxed)) { // `head_` 的 ptr ref 转移到 `new_node` 的 `next_` 上 .
      ;
    }
  }

  std::optional<ValType> pop() {
    TaggedPtr<Node> old_head{head_.load(std::memory_order_relaxed)};
    while (true) {                       // inf loop 以便灵活控制 CAS 成功/失败 的不同行为
      increase_headptr_refcnt(old_head); // 成功 acquire `head_` 并增加 ptr refcnt
      Node *old_head_ptr{old_head.get_ptr()};
      if (!old_head_ptr) {
        return std::nullopt;
      }

      if (head_.compare_exchange_strong(old_head, old_head_ptr->next_, std::memory_order_relaxed,
                                        std::memory_order_relaxed)) { // 尝试独占 `head_` 指向的资源
        ValType ret{std::move(old_head_ptr->val_)};                   // 拿到数据, 可以放弃独占当前资源的 1 个 ref
        uint16_t move_refcnt{
            static_cast<uint16_t>(old_head.get_tag() - 2)}; // 减少 `head_` ptr 的 1 个 ref + 当前独占资源的 1 个 ref
        if (old_head_ptr->in_refcnt.fetch_add(move_refcnt, std::memory_order_release) ==
            -move_refcnt) { // 将 ptr refcnt 加回资源 refcnt
          delete old_head_ptr;
        }
        return std::make_optional(std::move(ret));
      } else if (old_head_ptr->in_refcnt.fetch_sub(1, std::memory_order_relaxed) ==
                 1) { // 独占失败, 直接在资源上减少 ref
        old_head_ptr->in_refcnt.load(std::memory_order_acquire);
        delete old_head_ptr;
      }
    }
  }
};

#define VALTAG_SCALE 1000000u

void lockfree_stack_test() {
  std::vector<int> valtag(VALTAG_SCALE, 0);
  LockFreeStack<int> stack{};

  std::barrier b{PARA_CNT};
  std::vector<std::thread> js(PARA_CNT);

  for (int i = 0; i < PARA_CNT; i++) {
    js[i] = std::thread{[&valtag, &stack, &b, i]() {
      b.arrive_and_wait();
      unsigned int blksz{(VALTAG_SCALE + PARA_CNT - 1) / PARA_CNT};
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
  std::vector<int> stack(VALTAG_SCALE, 0);
  std::mutex m{};

  std::barrier b{PARA_CNT};
  std::vector<std::thread> js(PARA_CNT);

  for (int i = 0; i < PARA_CNT; i++) {
    js[i] = std::thread{[&valtag, &stack, &m, &b, i]() {
      b.arrive_and_wait();
      unsigned int blksz{(VALTAG_SCALE + PARA_CNT - 1) / PARA_CNT};
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
  // std::vector<int> stack(VALTAG_SCALE, 0);
  boost::lockfree::stack<int> stack{VALTAG_SCALE};

  std::barrier b{PARA_CNT};
  std::vector<std::thread> js(PARA_CNT);

  for (int i = 0; i < PARA_CNT; i++) {
    js[i] = std::thread{[&valtag, &stack, &b, i]() {
      b.arrive_and_wait();
      unsigned int blksz{(VALTAG_SCALE + PARA_CNT - 1) / PARA_CNT};
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

  // auto beg3{std::chrono::high_resolution_clock::now()};
  // boost_stack_test();
  // auto end3{std::chrono::high_resolution_clock::now()};
  // std::cout << end3 - beg3 << std::endl;

  auto beg1{std::chrono::high_resolution_clock::now()};
  normal_stack_test();
  auto end1{std::chrono::high_resolution_clock::now()};
  std::cout << end1 - beg1 << std::endl;
}