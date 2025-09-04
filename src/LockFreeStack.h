#include "SimpleCU_HazPtr.h"
#include <bits/stdc++.h>

template<typename ValType, typename Allocator = std::allocator<ValType>>
class LockFreeStack {
private:
  struct Node {
    ValType val_;
    Node *next_;
    Node(const ValType &val) : val_{val} {
    }
  };
  std::atomic<Node *> head_;
  SimpleCU::HazPtr::HazPtrManager<Node, 20, 1> hazptr_manager_;

  using NodeAllocTraits = std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<Node>>;
  using NodeAlloc = typename NodeAllocTraits::allocator_type;

  NodeAlloc allocator_;

public:
  void push(const ValType &val) {
    // Node *new_node{new Node{val}};
    Node *new_node{NodeAllocTraits::allocate(allocator_, 1)};
    NodeAllocTraits::construct(allocator_, new_node, val);
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