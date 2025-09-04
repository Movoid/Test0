#pragma once
#include "SimpleCU_Utils.h"
#include <bits/stdc++.h>

namespace SimpleCU::QSBR::Details {

  template<typename ValType, typename DeleterType>
  class RetiredContext {
  private:
    using masked_epoch_t = std::uint16_t;
    using ctx_idx_t_ = std::uint16_t;
    using CriticalEpochSnapshot_ = std::vector<std::pair<ctx_idx_t_, masked_epoch_t>>;

    template<std::size_t Size>
    using FullEpochSnapshot_ = std::array<masked_epoch_t, Size>;

    struct RetiredNode {
      ValType val_;
      CriticalEpochSnapshot_ critical_snapshot_;
      RetiredNode *next_;
    };

    RetiredNode *retired_{};
    std::atomic<std::uint64_t> cnt_{};

  public:
    RetiredContext() = default;
    ~RetiredContext() {
      RetiredNode *old_retired{retired_};
      while (old_retired) {
        RetiredNode *next{old_retired->next_};
        // delete_val(std::move(old_retired->val_));
        delete old_retired;
        old_retired = next;
      }
    }
    RetiredContext(const RetiredContext &obj) = delete;
    RetiredContext &operator=(const RetiredContext &obj) = delete;
    RetiredContext(RetiredContext &&obj) = delete;
    RetiredContext &operator=(RetiredContext &&obj) = delete;

    auto get_cnt() -> std::uint64_t {
      return cnt_.load(std::memory_order_relaxed);
    }

    void retire(ValType &&val, CriticalEpochSnapshot_ &&snapshot) {
      RetiredNode *new_node{new RetiredNode{std::move(val), std::move(snapshot), retired_}};
      retired_ = new_node;
      cnt_.fetch_add(1, std::memory_order_relaxed);
    }

    template<std::size_t Size, typename DeleterType_ = DeleterType>
    void reclaim(const FullEpochSnapshot_<Size> &latest_snapshot, DeleterType_ &&deleter) {
      RetiredNode *old_retired{retired_};
      std::uint64_t unsafe_cnt{};
      retired_ = nullptr;
      cnt_.store(0, std::memory_order_relaxed);
      while (old_retired) {
        RetiredNode *next{old_retired->next_};
        bool is_unsafe{};
        for (auto &rec : old_retired->critical_snapshot_) {
          if (latest_snapshot[rec.first] == rec.second) { // 若 Epoch 为奇数且没变则 unsafe
            is_unsafe = true;
            break;
          }
        }
        if (is_unsafe) {
          old_retired->next_ = retired_;
          retired_ = old_retired;
          unsafe_cnt++;
        } else {
          deleter(std::move(old_retired->val_));
          delete old_retired;
        }
        old_retired = next;
      }
      cnt_.fetch_add(unsafe_cnt, std::memory_order_relaxed);
    }
  };

}; // namespace SimpleCU::QSBR::Details

namespace SimpleCU::QSBR {

  /**
   * @brief Quiescent-State Based Reclamation.
   *
   * @tparam ThreadCnt 最大线程数.
   * @tparam ValType 受管理的确切类型.
   * @tparam DeleterType 自定义 deleter.
   */
  template<std::size_t ThreadCnt, typename ValType, typename DeleterType = Utils::DefaultDeleter<ValType>>
  class QSBRManager : private Utils::EBODeleterStorage<DeleterType> {
  private:
    using DeleterStorage_ = Utils::EBODeleterStorage<DeleterType>;

    using epoch_t_ = std::uint32_t;
    using masked_epoch_t = std::uint16_t;
    using ctx_idx_t_ = std::uint16_t;
    using mgr_idx_t_ = std::uint64_t;

    using Epoch_ = std::atomic<epoch_t_>;
    using RetiredContext_ = Details::RetiredContext<ValType, DeleterType>;
    using QSBRContext_ = Utils::Aligned<std::pair<Epoch_, RetiredContext_>>;

    using CriticalEpochSnapshot_ = std::vector<std::pair<ctx_idx_t_, masked_epoch_t>>;

    template<std::size_t Size>
    using FullEpochSnapshot_ = std::array<masked_epoch_t, Size>;

    constexpr static std::uint64_t epoch_mask{(1ull << 16) - 1};

    /**
     * `Epoch_` 和 `RetiredContext_` 共用 Cacheline .
     */
    std::unique_ptr<std::array<QSBRContext_, ThreadCnt>> ctxs_;
    std::atomic<ctx_idx_t_> next_ctx_idx_{};

    struct LocalEntry {
      QSBRContext_ *local_qsbr_ctx_;
    };

    inline static std::atomic<mgr_idx_t_> next_mgr_idx_{};
    const mgr_idx_t_ mgr_idx_;
    thread_local inline static std::unordered_map<mgr_idx_t_, LocalEntry> tls_map_;

    /** 奇数 Epoch 则此线程位于临界区. */
    auto is_critical_epoch(epoch_t_ epoch) -> bool {
      return epoch % 2 == 1;
    }

    auto get_context() -> std::optional<LocalEntry> {
      auto iter{tls_map_.find(mgr_idx_)};
      if (iter != tls_map_.end()) {
        return std::make_optional(iter->second);
      }
      // Register this new thread.
      ctx_idx_t_ cur_ctx_idx_{next_ctx_idx_.load(std::memory_order_relaxed)};
      do {
        if (cur_ctx_idx_ >= ThreadCnt) {
          return std::nullopt;
        }
      } while (!next_ctx_idx_.compare_exchange_weak(cur_ctx_idx_, cur_ctx_idx_ + 1, std::memory_order_release,
                                                    std::memory_order_relaxed));
      // Registered.
      tls_map_[mgr_idx_] = LocalEntry{&(*ctxs_)[cur_ctx_idx_]};
      return tls_map_[mgr_idx_];
    }

    auto snapshot_critical_epochs() -> CriticalEpochSnapshot_ {
      ctx_idx_t_ end_idx{next_ctx_idx_.load(std::memory_order_acquire)};
      CriticalEpochSnapshot_ snapshot{};
      snapshot.reserve(ThreadCnt / 2);
      for (ctx_idx_t_ i = 0; i < end_idx; i++) {
        masked_epoch_t epoch_i{
            static_cast<masked_epoch_t>((*ctxs_)[i].first.load(std::memory_order_acquire) & epoch_mask)};
        if (is_critical_epoch(epoch_i)) {
          snapshot.emplace_back(std::make_pair(i, epoch_i));
        }
      }
      return snapshot;
    }

    /** 栈上分配定长的 `FullEpochSnapshot_<ThreadCnt>` 避免 `new` 带来的锁开销. */
    auto snapshot_full_epochs() -> FullEpochSnapshot_<ThreadCnt> {
      ctx_idx_t_ end_idx{next_ctx_idx_.load(std::memory_order_acquire)};
      FullEpochSnapshot_<ThreadCnt> snapshot{};
      for (ctx_idx_t_ i = 0; i < end_idx; i++) {
        snapshot[i] = static_cast<masked_epoch_t>((*ctxs_)[i].first.load(std::memory_order_acquire) & epoch_mask);
      }
      return snapshot;
    }

  public:
    /**
     * 此处已提前构造所有的 Epoch 等元数据,
     * 在 `get_context` 注册流程中, 修改了 idx 后没有更多操作,
     * 不存在 idx 后操作对其他线程尚未可见的问题.
     */
    QSBRManager()
        : ctxs_{std::make_unique<std::array<QSBRContext_, ThreadCnt>>()},
          mgr_idx_{next_mgr_idx_.fetch_add(1, std::memory_order_relaxed)} {
      for (ctx_idx_t_ i = 0; i < ThreadCnt; i++) {
        (*ctxs_)[i].first.store(0, std::memory_order_relaxed);
      }
    }

    /**
     * 如果 `DeleterType_` 的 `operator()` 是非 `const` 的, 则无法使用 `const T&` 的形参, 而是要 `T&` (STL 也如此).
     * 使用转发引用能很好处理 `const` 和左右值.
     */
    template<typename DeleterType_ = DeleterType,
             typename Requires = std::enable_if_t<std::is_same_v<std::remove_reference_t<DeleterType_>, DeleterType>>>
    QSBRManager(DeleterType_ &&deleter)
        : DeleterStorage_{deleter}, ctxs_{std::make_unique<std::array<QSBRContext_, ThreadCnt>>()},
          mgr_idx_{next_mgr_idx_.fetch_add(1, std::memory_order_relaxed)} {
      for (ctx_idx_t_ i = 0; i < ThreadCnt; i++) {
        (*ctxs_)[i].first.store(0, std::memory_order_relaxed);
      }
    }

    QSBRManager(const QSBRManager &) = delete;
    auto operator=(const QSBRManager &) -> QSBRManager & = delete;
    QSBRManager(QSBRManager &&) = delete;
    auto operator=(QSBRManager &&) -> QSBRManager & = delete;

    /**
     * 析构 `RetiredContext_` 成员时会 reclaim.
     * `QSBRManager` 的生命周期应该晚于所有线程结束.
     */
    ~QSBRManager() {
      tls_map_.erase(mgr_idx_);
    }

    /**
     * 无论是 `enter` 还是 `exit` 都 Epoch++.
     */
    auto enter_critical_zone() -> bool {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      Epoch_ &local_epoch{ctx->first};
      if (local_epoch.fetch_add(1, std::memory_order_acquire) % 2 == 1) {
        return false;
      }
      return true;
    }

    auto exit_critical_zone() -> bool {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      Epoch_ &local_epoch{ctx->first};
      if (local_epoch.fetch_add(1, std::memory_order_release) % 2 == 0) {
        return false;
      }
      return true;
    }

    auto get_retired_cnt_local() -> std::uint64_t {
      auto context{get_context()};
      if (!context.has_value()) {
        return 0;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      return retired_ctx.get_cnt();
    }

    void retire(ValType &&val) {
      auto context{get_context()};
      if (!context.has_value()) {
        return;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      retired_ctx.retire(std::move(val), snapshot_critical_epochs());
    }

    void reclaim_local() {
      auto context{get_context()};
      if (!context.has_value()) {
        return;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      retired_ctx.reclaim(snapshot_full_epochs(), this->get_deleter());
    }
  };

  /**
   * RAII Guard.
   */
  template<std::size_t ThreadCnt, typename ValType, typename DeleterType>
  class QSBRGuard {
  private:
    using QSBRManager_ = QSBRManager<ThreadCnt, ValType, DeleterType>;
    QSBRManager_ *mgr_;

  public:
    QSBRGuard(QSBRManager_ &mgr) : mgr_{&mgr} {
      mgr_->enter_critical_zone();
    }
    QSBRGuard(const QSBRGuard &) = delete;
    auto operator=(const QSBRGuard &) -> QSBRGuard & = delete;

    QSBRGuard(QSBRGuard &&that) noexcept {
      mgr_ = that.mgr_;
      that.mgr_ = nullptr;
    }

    auto operator=(QSBRGuard &&that) noexcept -> QSBRGuard & {
      if (&this == that) return *this;
      if (mgr_) {
        mgr_->exit_critical_zone();
      }
      mgr_ = that.mgr_;
      that = nullptr;
      return *this;
    }

    ~QSBRGuard() {
      if (mgr_) {
        mgr_->exit_critical_zone();
      }
    }
  };

} // namespace SimpleCU::QSBR