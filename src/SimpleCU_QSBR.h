#pragma once
#include "SimpleCU_Utils.h"
#include <bits/stdc++.h>

namespace SimpleCU::QSBR::Details {

  template<typename ValType, typename DeleterType = Utils::DefaultDeleter<ValType>>
  class RetiredContext : private Utils::EBODeleterStorage<DeleterType> {
  private:
    struct RetiredNode {
      ValType val_;
      RetiredNode *next_;
    };

    void delete_val(ValType &&val_) {
      this->get_deleter()(std::move(val_));
    }

  public:
  };

}; // namespace SimpleCU::QSBR::Details

namespace SimpleCU::QSBR {

  template<typename ValType, typename DeleterType, std::size_t ThreadCnt>
  class QSBRManager {
  private:
    using Epoch_ = std::atomic<std::uint64_t>;
    using RetiredContext_ = Details::RetiredContext<ValType, DeleterType>;
    using QSBRContext_ = Utils::Aligned<std::pair<Epoch_, RetiredContext_>>;

    std::vector<QSBRContext_> ctxs_;

    struct LocalEntry {
      Epoch_ *local_epoch_;
      RetiredContext_ *local_retired_ctx_;
    };

    inline static std::atomic<std::uint64_t> next_idx_;
    const std::uint64_t this_idx_;
    thread_local inline static std::unordered_map<std::uint64_t, LocalEntry> tls_map_;

    auto get_context() -> std::optional<LocalEntry> {
      auto iter{tls_map_.find(this_idx_)};
      if (iter != tls_map_.end()) {
        return *iter;
      }
    }

  public:
    void enter_critical_zone();
    void exit_critical_zone();
    void reclaim_local();
  };

} // namespace SimpleCU::QSBR