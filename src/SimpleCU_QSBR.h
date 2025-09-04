#pragma once
#include "SimpleCU_Utils.h"
#include <bits/stdc++.h>

namespace SimpleCU::QSBR::Details {

  template<typename ValType, typename DeleterType>
  class RetiredContext : private Utils::EBODeleterStorage<DeleterType> {
  private:
    using epoch_t_ = std::uint64_t;
    using ctx_idx_t_ = std::uint64_t;
    using CriticalEpochSnapshot_ = std::vector<std::pair<ctx_idx_t_, epoch_t_>>;

    template<std::size_t Size>
    using FullEpochSnapshot_ = std::array<epoch_t_, Size>;

    struct RetiredNode {
      ValType val_;
      CriticalEpochSnapshot_ critical_snapshot_;
      RetiredNode *next_;
    };

    RetiredNode *retired_{};
    Utils::Aligned<std::atomic<std::uint64_t>> cnt_{};

    void delete_val(ValType &&val_) {
      this->get_deleter()(std::move(val_));
    }

  public:
    RetiredContext() = default;
    ~RetiredContext() {
      RetiredNode *old_retired{retired_};
      while (old_retired) {
        RetiredNode *next{old_retired->next_};
        delete_val(std::move(old_retired->val_));
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

    template<std::size_t Size>
    void reclaim(const FullEpochSnapshot_<Size> &latest_snapshot) {
      RetiredNode *old_retired{retired_};
      std::uint64_t unsafe_cnt{};
      retired_ = nullptr;
      cnt_.store(0, std::memory_order_relaxed);
      while (old_retired) {
        RetiredNode *next{old_retired->next_};
        bool is_unsafe{};
        for (auto &rec : old_retired->critical_snapshot_) {
          if (latest_snapshot[rec.first] == rec.second) {
            is_unsafe = true;
            break;
          }
        }
        if (is_unsafe) {
          old_retired->next_ = retired_;
          retired_ = old_retired;
          unsafe_cnt++;
        } else {
          delete_val(std::move(old_retired->val_));
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
   * Comments todo.
   */
  template<std::size_t ThreadCnt, typename ValType, typename DeleterType = Utils::DefaultDeleter<ValType>>
  class QSBRManager {
  private:
    using epoch_t_ = std::uint64_t;
    using Epoch_ = std::atomic<epoch_t_>;
    using RetiredContext_ = Details::RetiredContext<ValType, DeleterType>;
    using QSBRContext_ = Utils::Aligned<std::pair<Epoch_, RetiredContext_>>;
    using ctx_idx_t_ = std::uint64_t;
    using mgr_idx_t_ = std::uint64_t;
    using CriticalEpochSnapshot_ = std::vector<std::pair<ctx_idx_t_, epoch_t_>>;

    template<std::size_t Size>
    using FullEpochSnapshot_ = std::array<epoch_t_, Size>;

    std::unique_ptr<std::array<QSBRContext_, ThreadCnt>> ctxs_;
    std::atomic<ctx_idx_t_> next_ctx_idx_{};

    struct LocalEntry {
      QSBRContext_ *local_qsbr_ctx_;
    };

    inline static std::atomic<mgr_idx_t_> next_mgr_idx_{};
    const mgr_idx_t_ mgr_idx_;
    thread_local inline static std::unordered_map<mgr_idx_t_, LocalEntry> tls_map_;

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
        epoch_t_ epoch_i{(*ctxs_)[i].first.load(std::memory_order_acquire)};
        if (is_critical_epoch(epoch_i)) {
          snapshot.emplace_back(std::make_pair(i, epoch_i));
        }
      }
      return snapshot;
    }

    auto snapshot_full_epochs() -> FullEpochSnapshot_<ThreadCnt> {
      ctx_idx_t_ end_idx{next_ctx_idx_.load(std::memory_order_acquire)};
      FullEpochSnapshot_<ThreadCnt> snapshot{};
      for (ctx_idx_t_ i = 0; i < end_idx; i++) {
        snapshot[i] = (*ctxs_)[i].first.load(std::memory_order_acquire);
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
    }

    QSBRManager(const QSBRManager &) = delete;
    auto operator=(const QSBRManager &) -> QSBRManager & = delete;
    QSBRManager(QSBRManager &&) = delete;
    auto operator=(QSBRManager &&) -> QSBRManager & = delete;

    ~QSBRManager() { // 析构 RetiredContext 时会 reclaim.
      tls_map_.erase(mgr_idx_);
    }

    auto enter_critical_zone() -> bool {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      Epoch_ &local_epoch{ctx->first};
      local_epoch.fetch_add(1, std::memory_order_acquire);
      return true;
    }

    auto exit_critical_zone() -> bool {
      auto context{get_context()};
      if (!context.has_value()) {
        return false;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      Epoch_ &local_epoch{ctx->first};
      local_epoch.fetch_add(1, std::memory_order_release);
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
      retired_ctx.reclaim(snapshot_full_epochs());
    }
  };

} // namespace SimpleCU::QSBR