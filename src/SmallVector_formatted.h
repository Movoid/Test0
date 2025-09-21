// ============================================================
//
// SmallVector.h
//
// ============================================================

#pragma once
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <type_traits>

namespace meow_utils {
  /**
   * SmallVector for faster stack allocations.
   */
  template<typename ValType, std::uint64_t BufferSize, typename Alloc = std::allocator<ValType>>
  class SmallVector {
  private:
    alignas(ValType) std::byte inline_data_[sizeof(ValType) * BufferSize];
    ValType *data_pos_{reinterpret_cast<ValType *>(inline_data_)};
    std::uint64_t size_{};
    std::uint64_t capacity_{BufferSize};
    bool is_inline_{true};
    Alloc alloc_;

    static constexpr std::uint64_t INCREMENT_FACTOR{2};
    static constexpr std::uint64_t MAX_SIZE{std::numeric_limits<std::ptrdiff_t>::max() / sizeof(ValType)};

    using allocator_traits_ = std::allocator_traits<Alloc>;

    static constexpr bool VAL_COPY_MOVE_CTOR_NOEXCEPT{std::is_nothrow_copy_constructible_v<ValType> ||
                                                      std::is_nothrow_move_constructible_v<ValType>};
    static constexpr bool VAL_COPY_MOVE_ASSIGN_NOEXCEPT{std::is_nothrow_copy_assignable_v<ValType> ||
                                                        std::is_nothrow_move_assignable_v<ValType>};
    static constexpr bool VAL_DTOR_NOEXCEPT{std::is_nothrow_destructible_v<ValType>};

    static constexpr bool ALLOC_COPY_CTOR_NOEXCEPT{std::is_nothrow_copy_constructible_v<Alloc>};
    static constexpr bool ALLOC_MOVE_CTOR_NOEXCEPT{std::is_nothrow_move_constructible_v<Alloc>};
    static constexpr bool ALLOC_COPY_ASSIGN_NOEXCEPT{std::is_nothrow_copy_assignable_v<Alloc>};
    static constexpr bool ALLOC_MOVE_ASSIGN_NOEXCEPT{std::is_nothrow_move_assignable_v<Alloc>};

    static constexpr bool ALLOC_SELECT_NOEXCEPT{
        noexcept(allocator_traits_::select_on_container_copy_construction(std::declval<Alloc>()))};

    template<bool IsConst>
    class IteratorImpl {
    private:
      std::conditional_t<IsConst, const ValType *, ValType *> ptr_;

    public:
      using difference_type = std::ptrdiff_t;
      using value_type = ValType;
      using pointer = std::conditional_t<IsConst, const ValType *, ValType *>;
      using reference = std::conditional_t<IsConst, const ValType &, ValType &>;
      using iterator_category = std::random_access_iterator_tag;

      IteratorImpl() noexcept = default;
      explicit IteratorImpl(const pointer &ptr) noexcept : ptr_{ptr} {
      }

      IteratorImpl(const IteratorImpl &) noexcept = default;
      IteratorImpl(IteratorImpl &&) noexcept = default;
      auto operator=(const IteratorImpl &) noexcept -> IteratorImpl & = default;
      auto operator=(IteratorImpl &&) noexcept -> IteratorImpl & = default;
      ~IteratorImpl() noexcept = default;

      template<bool IsConst_>
      friend class Iterator_;

      template<bool IsConst_,
               typename Requires_ = std::enable_if_t<!std::is_base_of_v<IteratorImpl, IteratorImpl<IsConst_>>>>
      explicit IteratorImpl(const IteratorImpl<IsConst_> &that) noexcept : ptr_{that.ptr_} {
      }

      auto operator*() const noexcept -> const ValType & {
        return *ptr_;
      }

      auto operator*() noexcept -> ValType & {
        return *ptr_;
      }

      auto operator->() const noexcept -> const ValType * {
        return ptr_;
      }

      auto operator->() noexcept -> ValType * {
        return ptr_;
      }

      auto operator++() noexcept -> IteratorImpl & {
        ++ptr_;
        return *this;
      }
      auto operator++(int) noexcept -> IteratorImpl {
        IteratorImpl tmp{*this};
        ++(*this);
        return tmp;
      }
      auto operator--() noexcept -> IteratorImpl & {
        --ptr_;
        return *this;
      }
      auto operator--(int) noexcept -> IteratorImpl {
        IteratorImpl tmp{*this};
        --(*this);
        return tmp;
      }
      auto operator+=(difference_type n) noexcept -> IteratorImpl & {
        ptr_ += n;
        return *this;
      }

      auto operator-=(difference_type n) noexcept -> IteratorImpl & {
        ptr_ -= n;
        return *this;
      }

      auto operator[](std::ptrdiff_t n) const noexcept -> const ValType & {
        return *(*this + n);
      }

      auto operator[](std::ptrdiff_t n) noexcept -> ValType & {
        return *(*this + n);
      }

      auto operator+(std::ptrdiff_t n) const noexcept -> IteratorImpl {
        IteratorImpl tmp{*this};
        tmp += n;
        return tmp;
      }

      auto operator-(std::ptrdiff_t n) const noexcept -> IteratorImpl {
        IteratorImpl tmp{*this};
        tmp -= n;
        return tmp;
      }

      template<bool IsConstA, bool IsConstB>
      friend auto operator==(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool;
      template<bool IsConstA, bool IsConstB>
      friend auto operator!=(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool;
      template<bool IsConstA, bool IsConstB>
      friend auto operator<(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool;
      template<bool IsConstA, bool IsConstB>
      friend auto operator>(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool;
      template<bool IsConstA, bool IsConstB>
      friend auto operator<=(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool;
      template<bool IsConstA, bool IsConstB>
      friend auto operator>=(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool;

      template<bool IsConstA, bool IsConstB>
      friend auto operator-(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> std::ptrdiff_t;
    };

    template<bool IsConstA, bool IsConstB>
    friend auto operator==(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool {
      return a.ptr_ == b.ptr_;
    }
    template<bool IsConstA, bool IsConstB>
    friend auto operator!=(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool {
      return a.ptr_ != b.ptr_;
    }

    template<bool IsConstA, bool IsConstB>
    friend auto operator<(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool {
      return a.ptr_ < b.ptr_;
    }

    template<bool IsConstA, bool IsConstB>
    friend auto operator>(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool {
      return a.ptr_ > b.ptr_;
    }

    template<bool IsConstA, bool IsConstB>
    friend auto operator<=(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool {
      return a.ptr_ <= b.ptr_;
    }
    template<bool IsConstA, bool IsConstB>
    friend auto operator>=(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> bool {
      return a.ptr_ >= b.ptr_;
    }

    template<bool IsConstA, bool IsConstB>
    friend auto operator-(const IteratorImpl<IsConstA> &a, const IteratorImpl<IsConstB> &b) -> std::ptrdiff_t {
      return a.ptr_ - b.ptr_;
    }

  private:
    auto AllocHeap(std::uint64_t capacity) -> ValType * {
      return allocator_traits_::allocate(alloc_, capacity);
    }

    void DeallocHeap(ValType *heap_data, std::uint64_t capacity) {
      allocator_traits_::deallocate(alloc_, heap_data, capacity);
    }

    void DestructIn(ValType *pos, std::uint64_t size) noexcept(VAL_DTOR_NOEXCEPT) {
      if constexpr (!std::is_trivially_destructible_v<ValType>) {
        std::ptrdiff_t end{static_cast<std::ptrdiff_t>(size)};
        for (std::ptrdiff_t i = 0; i < end; i++) {
          allocator_traits_::destroy(alloc_, pos++);
        }
      }
    }

    void UninitCopyElements(ValType *dst, const ValType *src,
                            std::uint64_t size) noexcept(VAL_COPY_MOVE_CTOR_NOEXCEPT) {
      if constexpr (std::is_trivially_copyable_v<ValType>) {
        std::memcpy(dst, src, sizeof(ValType) * size);
      } else {
        std::ptrdiff_t end{static_cast<std::ptrdiff_t>(size)};
        if constexpr (std::is_nothrow_copy_constructible_v<ValType>) {
          for (std::ptrdiff_t i = 0; i < end; i++) {
            allocator_traits_::construct(alloc_, dst++, src[i]);
          }
        } else {
          std::ptrdiff_t i{};
          ValType *tmp{AllocHeap(size)};
          ValType *tmp_cur{};
          try {
            for (tmp_cur = tmp, i = 0; i < end; tmp_cur++, i++) {
              allocator_traits_::construct(alloc_, tmp_cur, src[i]);
            }
            for (tmp_cur = tmp, i = 0; i < end; tmp_cur++, i++, dst++) {
              allocator_traits_::construct(alloc_, dst, std::move(*tmp_cur));
            }
            DestructIn(tmp, size);
            DeallocHeap(tmp, size);
          } catch (...) {
            DestructIn(tmp, i);
            DeallocHeap(tmp, size);
            throw;
          }
        }
      }
    }

    void CopyElements(ValType *dst, const ValType *src, std::uint64_t size) noexcept(VAL_COPY_MOVE_ASSIGN_NOEXCEPT) {
      if constexpr (std::is_trivially_copyable_v<ValType>) {
        std::memcpy(dst, src, sizeof(ValType) * size);
      } else {
        std::ptrdiff_t end{static_cast<std::ptrdiff_t>(size)};
        if constexpr (std::is_nothrow_copy_assignable_v<ValType>) {
          for (std::ptrdiff_t i = 0; i < end; i++) {
            *dst++ = src[i];
          }
        } else {
          std::ptrdiff_t i{};
          ValType *tmp{AllocHeap(size)};
          ValType *tmp_cur{};
          try {
            for (tmp_cur = tmp, i = 0; i < end; tmp_cur++, i++) {
              allocator_traits_::construct(alloc_, tmp_cur, src[i]);
            }
            for (tmp_cur = tmp, i = 0; i < end; tmp_cur++, i++, dst++) {
              *dst = std::move(*tmp_cur);
            }
            DestructIn(tmp, size);
            DeallocHeap(tmp, size);
          } catch (...) {
            DestructIn(tmp, i);
            DeallocHeap(tmp, size);
            throw;
          }
        }
      }
    }

    void UninitMoveElements(ValType *dst, ValType *src, std::uint64_t size) noexcept(VAL_COPY_MOVE_CTOR_NOEXCEPT) {
      if constexpr (std::is_trivially_copyable_v<ValType>) {
        std::memmove(dst, src, sizeof(ValType) * size);
      } else {
        if constexpr (std::is_nothrow_move_constructible_v<ValType>) {
          for (std::uint64_t i = 0; i < size; i++) {
            allocator_traits_::construct(alloc_, dst++, std::move(src[i]));
          }
        } else {
          for (std::uint64_t i = 0; i < size; i++) {
            allocator_traits_::construct(alloc_, dst++, src[i]);
          }
        }
      }
    }

    void MoveElements(ValType *dst, ValType *src, std::uint64_t size) noexcept(VAL_COPY_MOVE_ASSIGN_NOEXCEPT) {
      if constexpr (std::is_trivially_copyable_v<ValType>) {
        std::memmove(dst, src, sizeof(ValType) * size);
      } else {
        if constexpr (std::is_nothrow_move_assignable_v<ValType>) {
          for (std::uint64_t i = 0; i < size; i++) {
            *dst++ = std::move(src[i]);
          }
        } else {
          for (std::uint64_t i = 0; i < size; i++) {
            *dst++ = src[i];
          }
        }
      }
    }

    void ExpandWith(std::ptrdiff_t offset, std::uint64_t with_size, std::uint64_t incre_factor = INCREMENT_FACTOR) {
      std::uint64_t needed_capacity{size_ + with_size};
      if (needed_capacity > MAX_SIZE) {
        throw std::length_error{"Exceed max size."};
      }
      std::ptrdiff_t end{static_cast<std::ptrdiff_t>(size_)};
      if (needed_capacity <= capacity_) [[likely]] {
        if (offset == end) [[likely]] { // Fast path for `emplace_back`.
          return;
        } else {
          bool is_overlapped{with_size < size_ - offset};
          if (!is_overlapped) [[likely]] {
            UninitMoveElements(data_pos_ + offset + with_size, data_pos_ + offset, size_ - offset);
          } else {
            UninitMoveElements(data_pos_ + size_, data_pos_ + size_ - with_size, with_size);
            MoveElements(data_pos_ + offset + with_size, data_pos_ + offset, size_ - offset - with_size);
          }
          DestructIn(data_pos_ + offset, with_size);
        }
      } else {
        std::uint64_t new_capacity{std::max(size_ * incre_factor, needed_capacity)};
        ValType *new_heap_data{AllocHeap(new_capacity)};
        UninitMoveElements(new_heap_data, data_pos_, offset);
        UninitMoveElements(new_heap_data + offset + with_size, data_pos_ + offset, size_ - offset);
        DestructIn(data_pos_, size_);
        if (!is_inline_) {
          DeallocHeap(data_pos_, capacity_);
        }
        is_inline_ = false;
        capacity_ = new_capacity;
        data_pos_ = new_heap_data;
      }
    }

    void ShrinkWithout(std::ptrdiff_t offset, std::uint64_t without_size) {
      std::uint64_t needed_capacity{size_ - without_size};
      ValType *inline_data{reinterpret_cast<ValType *>(inline_data_)};
      if (is_inline_) [[likely]] {
        MoveElements(inline_data + offset, inline_data + offset + without_size, size_ - (offset + without_size));
        DestructIn(inline_data + size_ - without_size, without_size);
      } else {
        if (needed_capacity > BufferSize) [[likely]] {
          MoveElements(data_pos_ + offset, data_pos_ + offset + without_size, size_ - (offset + without_size));
          DestructIn(data_pos_ + size_ - without_size, without_size);
        } else {
          UninitMoveElements(inline_data, data_pos_, offset);
          UninitMoveElements(inline_data + offset, data_pos_ + offset + without_size, size_ - (offset + without_size));
          DestructIn(data_pos_, size_);
          DeallocHeap(data_pos_, capacity_);
          is_inline_ = true;
          capacity_ = BufferSize;
          data_pos_ = inline_data;
        }
      }
    }

  public:
    using value_type = ValType;
    using reference = ValType &;
    using iterator = IteratorImpl<false>;
    using const_iterator = IteratorImpl<true>;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;

    using Iterator = IteratorImpl<false>;
    using ConstIterator = IteratorImpl<true>;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    SmallVector() = default;

    SmallVector(const SmallVector &that) noexcept(VAL_COPY_MOVE_CTOR_NOEXCEPT && ALLOC_SELECT_NOEXCEPT &&
                                                  ALLOC_MOVE_CTOR_NOEXCEPT) {
      auto alloc_select{allocator_traits_::select_on_container_copy_construction(that.alloc_)};
      ::new (std::addressof(alloc_)) Alloc{std::move(alloc_select)};
      if (!that.is_inline_) [[unlikely]] {
        data_pos_ = AllocHeap(that.capacity_);
      }
      UninitCopyElements(data_pos_, that.data_pos_, that.size_);
      size_ = that.size_;
      capacity_ = that.capacity_;
      is_inline_ = that.is_inline_;
    }

    SmallVector(SmallVector &&that) noexcept(VAL_COPY_MOVE_CTOR_NOEXCEPT && ALLOC_MOVE_CTOR_NOEXCEPT) {
      ::new (std::addressof(alloc_)) Alloc{std::move(that.alloc_)};
      if (that.is_inline_) [[likely]] {
        ValType *inline_data{reinterpret_cast<ValType *>(inline_data_)};
        ValType *that_inline_data{reinterpret_cast<ValType *>(that.inline_data_)};
        UninitMoveElements(inline_data, that_inline_data, that.size_);
      } else {
        data_pos_ = that.data_pos_;
        that.data_pos_ = nullptr;
      }
      size_ = that.size_;
      capacity_ = that.capacity_;
      is_inline_ = that.is_inline_;
    }

    auto operator=(const SmallVector &that) noexcept(VAL_COPY_MOVE_ASSIGN_NOEXCEPT && ALLOC_COPY_ASSIGN_NOEXCEPT)
        -> SmallVector & {
      if (this == std::addressof(that)) {
        return *this;
      }
      DestructIn(data_pos_, size_);
      size_ = 0;

      if constexpr (allocator_traits_::propagate_on_container_copy_assignment::value) {
        if constexpr (!allocator_traits_::is_always_equal::value) {
          if (alloc_ != that.alloc_) {
            if (!is_inline_) [[unlikely]] {
              DeallocHeap(data_pos_, capacity_);
              ValType *inline_data{reinterpret_cast<ValType *>(inline_data_)};
              data_pos_ = inline_data;
              capacity_ = BufferSize;
              is_inline_ = true;
            }
            alloc_ = that.alloc_;
          }
        }
      }

      if (that.size_ > capacity_) [[unlikely]] {
        data_pos_ = AllocHeap(that.capacity_);
        capacity_ = that.capacity_;
        is_inline_ = false;
      }
      UninitCopyElements(data_pos_, that.data_pos_, that.size_);
      size_ = that.size_;
      return *this;
    }

    auto operator=(SmallVector &&that) noexcept(VAL_COPY_MOVE_ASSIGN_NOEXCEPT && ALLOC_MOVE_ASSIGN_NOEXCEPT)
        -> SmallVector & {
      if (this == std::addressof(that)) {
        return *this;
      }
      DestructIn(data_pos_, size_);
      if constexpr (allocator_traits_::propagate_on_container_move_assignment::value) {
        if constexpr (!allocator_traits_::is_always_equal::value) {
          if (alloc_ != that.alloc_) {
            if (!is_inline_) [[unlikely]] {
              DeallocHeap(data_pos_, capacity_);
              ValType *inline_data{reinterpret_cast<ValType *>(inline_data_)};
              data_pos_ = inline_data;
              capacity_ = BufferSize;
              is_inline_ = true;
            }
            alloc_ = std::move(that.alloc_);
          }
        }
      }

      if (that.is_inline_) [[likely]] {
        UninitMoveElements(data_pos_, that.data_pos_, that.size_);
      } else {
        data_pos_ = that.data_pos_;
        that.data_pos_ = nullptr;
        capacity_ = that.capacity_;
        is_inline_ = false;
      }
      size_ = that.size_;
      return *this;
    }

    ~SmallVector() noexcept(VAL_DTOR_NOEXCEPT) {
      if (data_pos_) [[likely]] {
        DestructIn(data_pos_, size_);
        if (!is_inline_) [[unlikely]] {
          DeallocHeap(data_pos_, capacity_);
        }
      }
    }

    auto operator[](std::ptrdiff_t index) const noexcept -> const ValType & {
      return *std::launder(std::addressof(data_pos_[index]));
    }

    auto operator[](std::ptrdiff_t index) noexcept -> ValType & {
      return *std::launder(std::addressof(data_pos_[index]));
    }

    // NOLINTNEXTLINE
    auto begin() const noexcept -> ConstIterator {
      return std::launder(std::addressof(data_pos_[0]));
    }

    // NOLINTNEXTLINE
    auto end() const noexcept -> ConstIterator {
      return data_pos_ + size_;
    }
    // NOLINTNEXTLINE
    auto begin() noexcept -> Iterator {
      return std::launder(std::addressof(data_pos_[0]));
    }
    // NOLINTNEXTLINE
    auto end() noexcept -> Iterator {
      return data_pos_ + size_;
    }
    // NOLINTNEXTLINE
    auto cbegin() const noexcept -> ConstIterator {
      return std::launder(std::addressof(data_pos_[0]));
    }
    // NOLINTNEXTLINE
    auto cend() const noexcept -> ConstIterator {
      return data_pos_ + size_;
    }
    // NOLINTNEXTLINE
    auto rbegin() const noexcept -> ConstReverseIterator {
      return ConstReverseIterator{end()};
    }
    // NOLINTNEXTLINE
    auto rend() const noexcept -> ConstReverseIterator {
      return ConstReverseIterator{begin()};
    }
    // NOLINTNEXTLINE
    auto rbegin() noexcept -> ReverseIterator {
      return ReverseIterator{end()};
    }
    // NOLINTNEXTLINE
    auto rend() noexcept -> ReverseIterator {
      return ReverseIterator{begin()};
    }
    // NOLINTNEXTLINE
    auto crbegin() const noexcept -> ConstReverseIterator {
      return ConstReverseIterator{cend()};
    }
    // NOLINTNEXTLINE
    auto crend() const noexcept -> ConstReverseIterator {
      return ConstReverseIterator{cbegin()};
    }

    auto Size() const noexcept -> std::uint64_t {
      return size_;
    }

    auto Empty() const noexcept -> bool {
      return size_ == 0;
    }

    auto MaxSize() const noexcept -> std::uint64_t {
      return std::numeric_limits<std::ptrdiff_t>::max() / sizeof(ValType);
    }

    auto Capacity() const noexcept -> std::uint64_t {
      return capacity_;
    }

    auto Back() const noexcept -> const ValType & {
      return *std::launder(std::addressof(data_pos_[size_ - 1]));
    }

    auto Back() noexcept -> ValType & {
      return *std::launder(std::addressof(data_pos_[size_ - 1]));
    }

    auto Front() const noexcept -> const ValType & {
      return *std::launder(std::addressof(data_pos_[0]));
    }

    auto Front() noexcept -> ValType & {
      return *std::launder(std::addressof(data_pos_[0]));
    }

    template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
    auto Insert(ConstIterator pos, ValType_ &&val) -> Iterator {
      std::ptrdiff_t offset{pos - cbegin()};
      ExpandWith(offset, 1, INCREMENT_FACTOR);
      allocator_traits_::construct(alloc_, std::addressof(data_pos_[offset]), std::forward<ValType_>(val));
      ++size_;
      return Iterator{std::launder(std::addressof(data_pos_[offset]))};
    }

    auto Insert(ConstIterator pos, ConstIterator beg, ConstIterator end) -> Iterator {
      std::ptrdiff_t offset{pos - cbegin()};
      std::ptrdiff_t len{end - beg};
      ExpandWith(offset, len, INCREMENT_FACTOR);
      UninitCopyElements(std::addressof(data_pos_[offset]), std::addressof(*beg), len);
      size_ += len;
      return Iterator{std::launder(std::addressof(data_pos_[offset]))};
    }

    template<typename InputIter_,
             typename Requires_ = std::enable_if_t<!std::is_constructible_v<ConstIterator, InputIter_>>>
    auto Insert(ConstIterator pos, InputIter_ beg, InputIter_ end) -> Iterator {
      using iter_categroy = typename std::iterator_traits<InputIter_>::iterator_category;
      std::ptrdiff_t offset{pos - cbegin()};
      if constexpr (std::is_base_of_v<std::random_access_iterator_tag, iter_categroy>) {
        std::ptrdiff_t len{end - beg};
        ExpandWith(offset, len, INCREMENT_FACTOR);
        for (std::ptrdiff_t i = offset; beg != end; i++, beg++) {
          allocator_traits_::construct(alloc_, std::addressof(data_pos_[i]), *beg);
        }
        size_ += len;
      } else {
        SmallVector<ValType, BufferSize> tmp{};
        for (; beg != end; beg++) {
          tmp.EmplaceBack(*beg);
        }
        this->Insert(pos, tmp.begin(), tmp.end());
      }
      return Iterator{std::launder(std::addressof(data_pos_[offset]))};
    }

    template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
    auto Insert(ConstIterator pos, std::initializer_list<ValType_> list) -> Iterator {
      std::ptrdiff_t offset{pos - cbegin()};
      ExpandWith(offset, list.size(), INCREMENT_FACTOR);
      UninitCopyElements(std::addressof(data_pos_[offset]), list.begin(), list.size());
      size_ += list.size();
      return Iterator{std::launder(std::addressof(data_pos_[offset]))};
    }

    auto Erase(ConstIterator pos) -> Iterator {
      std::ptrdiff_t offset{pos - cbegin()};
      ShrinkWithout(offset, 1);
      --size_;
      return Iterator{std::launder(std::addressof(data_pos_[offset]))};
    }

    auto Erase(ConstIterator beg, ConstIterator end) -> Iterator {
      std::ptrdiff_t offset{beg - cbegin()};
      std::ptrdiff_t len{end - beg};
      ShrinkWithout(offset, len);
      size_ -= len;
      return Iterator{std::launder(std::addressof(data_pos_[offset]))};
    }

    template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
    void PushBack(ValType_ &&val) {
      ExpandWith(size_, 1, INCREMENT_FACTOR);
      allocator_traits_::construct(alloc_, std::addressof(data_pos_[size_]), std::forward<ValType_>(val));
      ++size_;
    }

    template<typename... Args, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, Args...>>>
    auto EmplaceBack(Args &&...args) -> ValType & {
      ExpandWith(size_, 1, INCREMENT_FACTOR);
      allocator_traits_::construct(alloc_, std::addressof(data_pos_[size_]), std::forward<Args>(args)...);
      return *std::launder(std::addressof(data_pos_[size_++]));
    }

    void PopBack() {
      ShrinkWithout(size_ - 1, 1);
      allocator_traits_::destroy(alloc_, std::addressof(data_pos_[size_]));
      --size_;
    }

    void Resize(std::uint64_t size) {
      if (size >= size_) [[likely]] {
        ExpandWith(size_, size - size_);
      } else {
        ShrinkWithout(size_, size_ - size);
      }
      std::ptrdiff_t end{static_cast<std::ptrdiff_t>(size)};
      for (std::ptrdiff_t i = size_; i < end; i++) {
        allocator_traits_::construct(alloc_, std::addressof(data_pos_[i]), ValType{});
      }
      size_ = size;
    }

    void Reserve(std::uint64_t capacity) {
      if (capacity < capacity_) {
        return;
      }
      ExpandWith(size_, capacity - size_, 1);
      capacity_ = capacity;
    }

    void Clear() noexcept {
      ShrinkWithout(0, size_);
      size_ = 0;
    }
  };
} // namespace meow_utils