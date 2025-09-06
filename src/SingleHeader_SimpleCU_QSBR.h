#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace simple_cu::utils {

  constexpr static std::size_t ALIGNMENT{std::hardware_constructive_interference_size};

  template<typename ValType, typename Requires = void>
  struct Aligned {};

  template<typename ValType>
  struct alignas(ALIGNMENT) Aligned<ValType, std::enable_if_t<std::is_class_v<ValType>>> : public ValType {
    using ValType::ValType;
    using ValType::operator=;
  };

  template<typename ValType>
  struct alignas(ALIGNMENT) Aligned<ValType, std::enable_if_t<!std::is_class_v<ValType>>> {
    ValType val_;

    Aligned() = default;
    explicit constexpr Aligned(const ValType &v) : val_(v) {
    }

    explicit operator ValType &() {
      return val_;
    }
    explicit operator const ValType &() const {
      return val_;
    }

    Aligned &operator=(const ValType &v) {
      val_ = v;
      return *this;
    }
  };

  template<typename ValType>
  using DefaultDeleter = std::default_delete<std::remove_pointer_t<ValType>>;

  template<typename DeleterType, typename Requires = void>
  class EBODeleterStorage {
  private:
    DeleterType deleter_;

  public:
    EBODeleterStorage() = default; // 如果非空 DeleterType 能空参构造, 则也允许
    explicit EBODeleterStorage(const DeleterType &deleter) : deleter_{deleter} { // 使用一个 DeleterType 对象构造
    }
    EBODeleterStorage(const EBODeleterStorage &) = default;
    EBODeleterStorage(EBODeleterStorage &&) = default;
    auto operator=(const EBODeleterStorage &) -> EBODeleterStorage & = default;
    auto operator=(EBODeleterStorage &&) -> EBODeleterStorage & = default;
    ~EBODeleterStorage() = default;

    auto GetDeleter() -> DeleterType & {
      return deleter_;
    }
  };

  template<typename DeleterType>
  class EBODeleterStorage<DeleterType, std::enable_if_t<std::is_empty_v<DeleterType>>> : public DeleterType {
  public:
    EBODeleterStorage() = default; // 空的 DeleterType 通常可以空参构造
    explicit EBODeleterStorage(const DeleterType &deleter)
        : DeleterType{deleter} { // 也允许使用一个已经构造了的空 DeleterType.
    }
    EBODeleterStorage(const EBODeleterStorage &) = default;
    EBODeleterStorage(EBODeleterStorage &&) = default;
    auto operator=(const EBODeleterStorage &) -> EBODeleterStorage & = default;
    auto operator=(EBODeleterStorage &&) -> EBODeleterStorage & = default;
    ~EBODeleterStorage() = default;

    auto GetDeleter() -> DeleterType & {
      return *this;
    }
  };
} // namespace simple_cu::utils

namespace simple_cu::qsbr::details {

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
    std::atomic<std::uint64_t> cnt_;

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

    auto GetCnt() -> std::uint64_t {
      return cnt_.load(std::memory_order_relaxed);
    }

    void Retire(ValType &&val, CriticalEpochSnapshot_ &&snapshot) {
      RetiredNode *new_node{new RetiredNode{std::move(val), std::move(snapshot), retired_}};
      retired_ = new_node;
      cnt_.fetch_add(1, std::memory_order_relaxed);
    }

    template<std::size_t Size, typename DeleterType_ = DeleterType>
    void Reclaim(const FullEpochSnapshot_<Size> &latest_snapshot, DeleterType_ &&deleter) {
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
          std::forward<DeleterType_>(deleter)(std::move(old_retired->val_));
          delete old_retired;
        }
        old_retired = next;
      }
      cnt_.fetch_add(unsafe_cnt, std::memory_order_relaxed);
    }
  };

}; // namespace simple_cu::qsbr::details

namespace simple_cu::qsbr {

  /**
   * @brief Quiescent-State Based Reclamation.
   *
   * @tparam ThreadCnt 最大线程数.
   * @tparam ValType 受管理的确切类型.
   * @tparam DeleterType 自定义 deleter.
   */
  template<std::size_t ThreadCnt, typename ValType, typename DeleterType = utils::DefaultDeleter<ValType>>
  class QSBRManager : private utils::EBODeleterStorage<DeleterType> {
  private:
    static_assert(ThreadCnt <= std::numeric_limits<std::uint16_t>::max(), "Too much threads.");
    using DeleterStorage_ = utils::EBODeleterStorage<DeleterType>;

    using epoch_t_ = std::uint32_t;
    using masked_epoch_t = std::uint16_t;
    using ctx_idx_t_ = std::uint16_t;
    using mgr_idx_t_ = std::uint64_t;

    using Epoch_ = std::atomic<epoch_t_>;
    using RetiredContext_ = details::RetiredContext<ValType, DeleterType>;
    using QSBRContext_ = utils::Aligned<std::pair<Epoch_, RetiredContext_>>;

    using CriticalEpochSnapshot_ = std::vector<std::pair<ctx_idx_t_, masked_epoch_t>>;

    template<std::size_t Size>
    using FullEpochSnapshot_ = std::array<masked_epoch_t, Size>;

    constexpr static std::uint64_t EPOCH_MASK{(1ULL << 16) - 1};

    /**
     * `Epoch_` 和 `RetiredContext_` 共用 Cacheline .
     */
    std::unique_ptr<std::array<QSBRContext_, ThreadCnt>> ctxs_;
    std::atomic<ctx_idx_t_> next_ctx_idx_;

    struct LocalEntry {
      QSBRContext_ *local_qsbr_ctx_;
    };

    inline static std::atomic<mgr_idx_t_> next_mgr_idx{};
    const mgr_idx_t_ mgr_idx_;
    thread_local inline static std::unordered_map<mgr_idx_t_, LocalEntry> tls_map;

    /** 奇数 Epoch 则此线程位于临界区. */
    auto IsCriticalEpoch(epoch_t_ epoch) -> bool {
      return (epoch & 1) == 1;
    }

    auto GetContext() -> std::optional<LocalEntry> {
      auto iter{tls_map.find(mgr_idx_)};
      if (iter != tls_map.end()) {
        return std::make_optional(iter->second);
      }
      // Register this new thread.
      ctx_idx_t_ cur_ctx_idx{next_ctx_idx_.load(std::memory_order_relaxed)};
      do {
        if (cur_ctx_idx >= ThreadCnt) {
          return std::nullopt;
        }
      } while (!next_ctx_idx_.compare_exchange_weak(cur_ctx_idx, cur_ctx_idx + 1, std::memory_order_release,
                                                    std::memory_order_relaxed));
      // Registered.
      tls_map[mgr_idx_] = LocalEntry{&(*ctxs_)[cur_ctx_idx]};
      return tls_map[mgr_idx_];
    }

    auto SnapshotCriticalEpochs() -> CriticalEpochSnapshot_ {
      ctx_idx_t_ end_idx{next_ctx_idx_.load(std::memory_order_acquire)};
      CriticalEpochSnapshot_ snapshot{};
      snapshot.reserve(end_idx / 2);
      for (ctx_idx_t_ i = 0; i < end_idx; i++) {
        masked_epoch_t epoch_i{
            static_cast<masked_epoch_t>((*ctxs_)[i].first.load(std::memory_order_acquire) & EPOCH_MASK)};
        if (IsCriticalEpoch(epoch_i)) {
          snapshot.emplace_back(std::make_pair(i, epoch_i));
        }
      }
      return snapshot;
    }

    /** 栈上分配定长的 `FullEpochSnapshot_<ThreadCnt>` 避免 `new` 带来的锁开销. */
    auto SnapshotFullEpochs() -> FullEpochSnapshot_<ThreadCnt> {
      ctx_idx_t_ end_idx{next_ctx_idx_.load(std::memory_order_acquire)};
      FullEpochSnapshot_<ThreadCnt> snapshot{};
      for (ctx_idx_t_ i = 0; i < end_idx; i++) {
        snapshot[i] = static_cast<masked_epoch_t>((*ctxs_)[i].first.load(std::memory_order_acquire) & EPOCH_MASK);
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
          mgr_idx_{next_mgr_idx.fetch_add(1, std::memory_order_relaxed)} {
      for (ctx_idx_t_ i = 0; i < static_cast<ctx_idx_t_>(ThreadCnt); i++) {
        (*ctxs_)[i].first.store(0, std::memory_order_relaxed);
      }
    }

    /**
     * 如果 `DeleterType_` 的 `operator()` 是非 `const` 的, 则无法使用 `const T&` 的形参, 而是要 `T&` (STL 也如此).
     * 使用转发引用能很好处理 `const` 和左右值.
     */
    template<typename DeleterType_ = DeleterType,
             typename Requires_ = std::enable_if_t<std::is_same_v<std::decay_t<DeleterType_>, DeleterType>>>
    explicit QSBRManager(DeleterType_ &&deleter)
        : DeleterStorage_{std::forward<DeleterType_>(deleter)},
          ctxs_{std::make_unique<std::array<QSBRContext_, ThreadCnt>>()},
          mgr_idx_{next_mgr_idx.fetch_add(1, std::memory_order_relaxed)} {
      for (ctx_idx_t_ i = 0; i < static_cast<ctx_idx_t_>(ThreadCnt); i++) {
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
      for (ctx_idx_t_ i = 0; i < static_cast<ctx_idx_t_>(ThreadCnt); i++) {
        RetiredContext_ &retired_ctx{(*ctxs_)[i].second};
        retired_ctx.Reclaim(SnapshotFullEpochs(), this->GetDeleter());
      }
      tls_map.erase(mgr_idx_);
    }

    /**
     * 无论是 `enter` 还是 `exit` 都 Epoch++.
     */
    auto EnterCriticalZone() -> bool {
      auto context{GetContext()};
      if (!context.has_value()) {
        return false;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      Epoch_ &local_epoch{ctx->first};
      return (local_epoch.fetch_add(1, std::memory_order_acquire) & 1ULL) != 1;
    }

    auto ExitCriticalZone() -> bool {
      auto context{GetContext()};
      if (!context.has_value()) {
        return false;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      Epoch_ &local_epoch{ctx->first};
      return (local_epoch.fetch_add(1, std::memory_order_release) & 1ULL) != 0;
    }

    auto GetRetiredCntLocal() -> std::uint64_t {
      auto context{GetContext()};
      if (!context.has_value()) {
        return 0;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      return retired_ctx.GetCnt();
    }

    void Retire(ValType &&val) {
      auto context{GetContext()};
      if (!context.has_value()) {
        return;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      retired_ctx.Retire(std::move(val), SnapshotCriticalEpochs());
    }

    void ReclaimLocal() {
      auto context{GetContext()};
      if (!context.has_value()) {
        return;
      }
      QSBRContext_ *ctx{context.value().local_qsbr_ctx_};
      RetiredContext_ &retired_ctx{ctx->second};
      retired_ctx.Reclaim(SnapshotFullEpochs(), this->GetDeleter());
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
    explicit QSBRGuard(QSBRManager_ &mgr) : mgr_{&mgr} {
      mgr_->EnterCriticalZone();
    }
    QSBRGuard(const QSBRGuard &) = delete;
    auto operator=(const QSBRGuard &) -> QSBRGuard & = delete;

    QSBRGuard(QSBRGuard &&that) noexcept {
      mgr_ = that.mgr_;
      that.mgr_ = nullptr;
    }

    auto operator=(QSBRGuard &&that) noexcept -> QSBRGuard & {
      if (this == &that) {
        return *this;
      }
      if (mgr_) {
        mgr_->ExitCriticalZone();
      }
      mgr_ = that.mgr_;
      that.mgr_ = nullptr;
      return *this;
    }

    ~QSBRGuard() {
      if (mgr_) {
        mgr_->ExitCriticalZone();
      }
    }
  };

} // namespace simple_cu::qsbr