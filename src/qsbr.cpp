#include "SimpleCU_Utils.h"
#include <bits/stdc++.h>

namespace SimpleCU::QSBR::Details {

  template<typename ValType, typename DeleterType>
  class RetiredContext : private Utils::EBODeleterStorage<DeleterType> {
  public:
    struct RetiredNode {
      ValType val_;
      RetiredNode *next_;
    };

    void delete_val(ValType &&val_) {
      this->get_deleter()(std::move(val_));
    }

    // todo.

  public:
    RetiredContext() = default;
    ~RetiredContext() {
      // 全部 reclaim. todo.
    }
    RetiredContext(const RetiredContext &obj) = delete;
    RetiredContext &operator=(const RetiredContext &obj) = delete;
    RetiredContext(RetiredContext &&obj) = delete;
    RetiredContext &operator=(RetiredContext &&obj) = delete;
  };

}; // namespace SimpleCU::QSBR::Details

namespace SimpleCU::QSBR {

  template<std::size_t ThreadCnt, typename ValType, typename DeleterType = Utils::DefaultDeleter<ValType>>
  class QSBRManager {
  private:
    using epoch_t_ = std::uint64_t;
    using Epoch_ = std::atomic<epoch_t_>;
    using RetiredContext_ = Details::RetiredContext<ValType, DeleterType>;
    using QSBRContext_ = Utils::Aligned<std::pair<Epoch_, RetiredContext_>>;
    using ctx_idx_t_ = std::uint64_t;
    using mgr_idx_t_ = std::uint64_t;

    std::unique_ptr<std::array<QSBRContext_, ThreadCnt>> ctxs_;
    std::atomic<ctx_idx_t_> next_ctx_idx_{};

    struct LocalEntry {
      QSBRContext_ *local_qsbr_ctx_;
    };

    inline static std::atomic<mgr_idx_t_> next_mgr_idx_{};
    const mgr_idx_t_ mgr_idx_;
    thread_local inline static std::unordered_map<mgr_idx_t_, LocalEntry> tls_map_;

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
      } while (next_ctx_idx_.compare_exchange_weak(cur_ctx_idx_, cur_ctx_idx_ + 1, std::memory_order_relaxed,
                                                   std::memory_order_relaxed));
      // Registered.
      tls_map_[mgr_idx_] = LocalEntry{&(*ctxs_)[cur_ctx_idx_]};
      return tls_map_[mgr_idx_];
    }

    auto snapshot_all_epochs() -> std::vector<std::pair<ctx_idx_t_, epoch_t_>> {
    }

  public:
    QSBRManager()
        : ctxs_{std::make_unique<std::array<QSBRContext_, ThreadCnt>>()},
          mgr_idx_{next_mgr_idx_.fetch_add(1, std::memory_order_relaxed)} {
    }
    ~QSBRManager() {
      // 这里应该阻塞操作所有的 QSBRContext_, 回收所有没有回收的. todo.
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
      Epoch_ &epoch{ctx->first};
      epoch.fetch_add(1, std::memory_order_acquire);
      return true;
    }

    void reclaim_local() {
      auto context{get_context()};
      if (!context.has_value()) {
        return;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      // todo
    }
  };

} // namespace SimpleCU::QSBR

int main() {

  SimpleCU::QSBR::QSBRManager<10, int> a{};
  a.enter_critical_zone();
}