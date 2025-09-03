#pragma once
#include <SimpleCU_Utils.h>
#include <bits/stdc++.h>

namespace SimpleCU::HazPtr::Details {

  namespace HazPtr {
    template<typename ValType, std::size_t SlotSize>
    class HazPtrContext {
    private:
      std::array<Utils::Aligned<std::atomic<const ValType *>>, SlotSize> hazptr_;

    public:
      bool contains(const ValType *ptr) const {
        for (std::size_t i = 0; i < SlotSize; i++) {
          if (hazptr_[i].load(std::memory_order_acquire) == ptr) {
            return true;
          }
        }
        return false;
      }

      bool set_hazptr(std::size_t idx, const ValType *ptr) {
        hazptr_[idx].store(ptr, std::memory_order_seq_cst);
        return true;
      }

      bool unset_hazptr(std::size_t idx) {
        hazptr_[idx].store(nullptr, std::memory_order_release);
        return true;
      }

      void output(std::unordered_set<const ValType *> &out) {
        for (std::size_t i = 0; i < SlotSize; i++) {
          const ValType *ptr{};
          if ((ptr = hazptr_[i].load(std::memory_order_acquire)) != nullptr) {
            out.insert(ptr);
          }
        }
      }
    };

    template<typename ValType>
    class RetiredContext {
    private:
      struct RetiredNode {
        ValType *val_;
        RetiredNode *next_;
      };
      RetiredNode *retired_;
      Utils::Aligned<std::atomic<std::size_t>> cnt_;

    public:
      RetiredContext() = default;
      ~RetiredContext() {
        RetiredNode *old_retired{retired_};
        while (old_retired) {
          RetiredNode *next{old_retired->next_};
          delete old_retired->val_;
          delete old_retired;
          old_retired = next;
        }
      }
      RetiredContext(const RetiredContext &obj) = delete;
      RetiredContext &operator=(const RetiredContext &obj) = delete;
      RetiredContext(RetiredContext &&obj) = delete;
      RetiredContext &operator=(RetiredContext &&obj) = delete;

      void retire(ValType *val) {
        RetiredNode *new_node{new RetiredNode{val, retired_}};
        retired_ = new_node;
        cnt_.fetch_add(1, std::memory_order_relaxed);
      }

      std::size_t get_cnt() const {
        return cnt_.load(std::memory_order_relaxed);
      }

      void delete_no_hazard(const std::unordered_set<const ValType *> &hazptrs) {
        RetiredNode *old_retired{retired_};
        retired_ = nullptr;
        cnt_.store(0, std::memory_order_relaxed);
        while (old_retired) {
          RetiredNode *next{old_retired->next_};
          if (!hazptrs.contains(old_retired->val_)) {
            delete old_retired->val_; // ?
            delete old_retired;
          } else {
            old_retired->next_ = retired_;
            retired_ = old_retired;
          }
          old_retired = next;
        }
      }
    };
  } // namespace HazPtr
} // namespace SimpleCU::HazPtr::Details

namespace SimpleCU::HazPtr {
  /**
   * @brief Hazard Pointer Manager.
   *
   * 含自动注册上下文.
   *
   * @tparam ValType 元素类型.
   * @tparam ThreadCnt 需要记录 Hazard Pointer 的线程总数.
   * @tparam SlotSize 每个线程需要的 Hazard Pointer 数量.
   */
  template<typename ValType, std::size_t ThreadCnt, std::size_t SlotSize>
  class HazPtrManager {
  private:
    using HazPtrContext_ = Utils::Aligned<Details::HazPtr::HazPtrContext<ValType, SlotSize>>;
    using RetiredContext_ = Utils::Aligned<Details::HazPtr::RetiredContext<ValType>>;

    std::array<Utils::Aligned<std::atomic<std::thread::id>>, ThreadCnt> ids_;
    std::array<Utils::Aligned<std::atomic<HazPtrContext_ *>>, ThreadCnt> hazptr_ctxs_;
    std::array<Utils::Aligned<std::atomic<RetiredContext_ *>>, ThreadCnt> retire_ctxs_;
    std::atomic<std::size_t> ctx_cnt_;

    /** Custom TLS specific to this object. */
    struct LocalEntry {
      HazPtrContext_ *local_hazptr_ctx_;
      RetiredContext_ *local_retired_ctx_;
    };
    inline static std::atomic<std::size_t> next_idx_;
    const std::size_t this_idx_;
    thread_local inline static std::vector<LocalEntry> tls_;

    /**
     * 每个线程 `get_context` 操作的 `tls_` 都是自己 thread_local 的,
     * `tls_.resize()` 是安全的.
     */
    std::optional<std::pair<HazPtrContext_ *, RetiredContext_ *>> get_context() {
      if (this_idx_ >= tls_.size()) {
        tls_.resize(this_idx_ + 1);
      }
      LocalEntry &ent{tls_[this_idx_]};
      if (ent.local_hazptr_ctx_) {
        return std::make_pair(ent.local_hazptr_ctx_, ent.local_retired_ctx_);
      }
      if (ctx_cnt_.load(std::memory_order_relaxed) >= ThreadCnt) {
        return std::nullopt;
      }
      std::thread::id local_id{std::this_thread::get_id()};
      for (std::size_t i = 0; i < ThreadCnt; i++) {
        HazPtrContext_ *tmp{};
        if (hazptr_ctxs_[i].compare_exchange_strong(tmp, reinterpret_cast<HazPtrContext_ *>(0xDEADBEEF),
                                                    std::memory_order_relaxed, std::memory_order_relaxed)) {
          ctx_cnt_.fetch_add(1, std::memory_order_relaxed);
          auto new_hazptr_ctx{new HazPtrContext_{}};
          auto new_retired_ctx{new RetiredContext_{}};
          ent.local_hazptr_ctx_ = new_hazptr_ctx;
          ent.local_retired_ctx_ = new_retired_ctx;
          hazptr_ctxs_[i] = new_hazptr_ctx;
          retire_ctxs_[i] = new_retired_ctx;
          ids_[i].store(std::this_thread::get_id(), std::memory_order_release);
          return std::make_pair(new_hazptr_ctx, new_retired_ctx);
        }
      }

      return std::nullopt;
    }

    std::unordered_set<const ValType *> collect_all_hazptrs() {
      std::unordered_set<const ValType *> res{};
      std::thread::id tmp{};
      for (std::size_t i = 0; i < ThreadCnt; i++) {
        if (ids_[i].load(std::memory_order_acquire) != tmp) {
          hazptr_ctxs_[i].load(std::memory_order_relaxed)->output(res);
        }
      }
      return res;
    }

  public:
    HazPtrManager() : this_idx_{next_idx_.fetch_add(1, std::memory_order_relaxed)} {
    }
    ~HazPtrManager() {
      for (std::size_t i = 0; i < ThreadCnt; i++) {
        delete hazptr_ctxs_[i].load(std::memory_order_relaxed);
      }
      for (std::size_t i = 0; i < ThreadCnt; i++) {
        delete retire_ctxs_[i].load(std::memory_order_relaxed);
      }
    }
    HazPtrManager(const HazPtrManager &obj) = delete;
    HazPtrManager &operator=(const HazPtrManager &obj) = delete;
    HazPtrManager(HazPtrManager &&obj) = delete;
    HazPtrManager &operator=(HazPtrManager &&obj) = delete;

    std::size_t get_max_hazptr_cnt_global() {
      return SlotSize * ThreadCnt;
    }

    std::size_t get_retired_cnt_local() {
      auto context{get_context()};
      if (!context.has_value()) {
        return 0;
      }
      RetiredContext_ *retired_ctx{context.value().second};
      return retired_ctx->get_cnt();
    }

    bool set_hazptr(std::size_t idx, const ValType *ptr) {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      HazPtrContext_ *hazptr_ctx{context.value().first};
      return hazptr_ctx->set_hazptr(idx, ptr);
    }

    bool unset_hazptr(std::size_t idx) {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      HazPtrContext_ *hazptr_ctx{context.value().first};
      return hazptr_ctx->unset_hazptr(idx);
    }

    void retire(ValType *ptr) {
      auto context{get_context()};
      if (!context.has_value()) {
        return;
      }
      RetiredContext_ *retired_ctx{context.value().second};
      retired_ctx->retire(ptr);
    }

    bool check_hazptr(const ValType *ptr) {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      HazPtrContext_ *hazptr_ctx{context.value().first};
      return hazptr_ctx->contains(ptr);
    }

    void reclaim_local() {
      auto context{get_context()};
      if (!context.has_value()) {
        return;
      }
      RetiredContext_ *retired_ctx{context.value().second};
      retired_ctx->delete_no_hazard(collect_all_hazptrs());
    }
  };
} // namespace SimpleCU::HazPtr
