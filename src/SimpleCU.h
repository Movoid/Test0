#include <bits/stdc++.h>

namespace SimpleCU::impl {

  template<typename ValType, std::size_t Alignment, typename Requires = void>
  struct AlignedWrapper {};

  template<typename ValType, std::size_t Alignment>
  struct alignas(Alignment)
      AlignedWrapper<ValType, Alignment, std::enable_if_t<std::is_class_v<ValType>>> : public ValType {
    using ValType::ValType;
    using ValType::operator=;
  };

  template<typename ValType, std::size_t Alignment>
  struct alignas(Alignment) AlignedWrapper<ValType, Alignment, std::enable_if_t<!std::is_class_v<ValType>>> {
    ValType val_;

    AlignedWrapper() = default;
    constexpr AlignedWrapper(const ValType &v) : val_(v) {
    }

    operator ValType &() {
      return val_;
    }
    operator const ValType &() const {
      return val_;
    }

    AlignedWrapper &operator=(const ValType &v) {
      val_ = v;
      return *this;
    }
  };

  template<typename ValType, std::size_t SlotSize>
  class HazPtrContext {
  private:
    std::array<AlignedWrapper<std::atomic<const ValType *>, 64>, SlotSize> hazptr_;

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
    AlignedWrapper<std::atomic<std::size_t>, 64> cnt_;

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
} // namespace SimpleCU::impl

namespace SimpleCU {

  template<typename ValType, std::size_t SlotCnt, std::size_t SlotSize>
  class HazPtrManager {
  private:
    constexpr static std::size_t ALIGNMENT{std::hardware_constructive_interference_size};
    using HazPtrContext_ = impl::AlignedWrapper<impl::HazPtrContext<ValType, SlotSize>, ALIGNMENT>;
    using RetiredContext_ = impl::AlignedWrapper<impl::RetiredContext<ValType>, ALIGNMENT>;

    std::array<impl::AlignedWrapper<std::atomic<std::thread::id>, ALIGNMENT>, SlotCnt> ids_;
    std::array<impl::AlignedWrapper<std::atomic<HazPtrContext_ *>, ALIGNMENT>, SlotCnt> hazptr_ctxs_;
    std::array<impl::AlignedWrapper<std::atomic<RetiredContext_ *>, ALIGNMENT>, SlotCnt> retire_ctxs_;
    std::atomic<std::size_t> ctx_cnt_;

    struct LocalEntry {
      HazPtrContext_ *local_hazptr_ctx_;
      RetiredContext_ *local_retired_ctx_;
    };

    inline static std::atomic<std::size_t> next_idx_;
    const std::size_t this_idx_;
    thread_local inline static std::vector<LocalEntry> tls_;

    std::optional<std::pair<HazPtrContext_ *, RetiredContext_ *>> get_context() {
      if (this_idx_ >= tls_.size()) {
        tls_.resize(this_idx_ + 1);
      }
      LocalEntry &ent{tls_[this_idx_]};
      if (ent.local_hazptr_ctx_) {
        return std::make_pair(ent.local_hazptr_ctx_, ent.local_retired_ctx_);
      }
      if (ctx_cnt_.load(std::memory_order_relaxed) >= SlotCnt) {
        return std::nullopt;
      }
      std::thread::id local_id{std::this_thread::get_id()};
      for (std::size_t i = 0; i < SlotCnt; i++) {
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
      for (std::size_t i = 0; i < SlotCnt; i++) {
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
      for (std::size_t i = 0; i < SlotCnt; i++) {
        delete hazptr_ctxs_[i].load(std::memory_order_relaxed);
      }
      for (std::size_t i = 0; i < SlotCnt; i++) {
        delete retire_ctxs_[i].load(std::memory_order_relaxed);
      }
    }
    HazPtrManager(const HazPtrManager &obj) = delete;
    HazPtrManager &operator=(const HazPtrManager &obj) = delete;
    HazPtrManager(HazPtrManager &&obj) = delete;
    HazPtrManager &operator=(HazPtrManager &&obj) = delete;

    std::size_t get_max_hazptr_cnt_global() {
      return SlotSize * SlotCnt;
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

    void delete_no_hazard_local() {
      auto context{get_context()};
      if (!context.has_value()) {
        return;
      }
      RetiredContext_ *retired_ctx{context.value().second};
      retired_ctx->delete_no_hazard(collect_all_hazptrs());
    }
  };
} // namespace SimpleCU
