#include <bits/stdc++.h>
using namespace std;

/**
 * SmallVector for faster stack storage.
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

  using allocator_traits_ = std::allocator_traits<Alloc>;

  template<bool IsConst>
  class Iterator_ {
  private:
    std::conditional_t<IsConst, const ValType *, ValType *> ptr_;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = ValType;
    using pointer = std::conditional_t<IsConst, const ValType *, ValType *>;
    using reference = std::conditional_t<IsConst, const ValType &, ValType &>;
    using iterator_category = std::random_access_iterator_tag;

    Iterator_() noexcept = default;
    Iterator_(const pointer &ptr) noexcept : ptr_{ptr} {
    }

    Iterator_(const Iterator_ &) noexcept = default;
    Iterator_(Iterator_ &&) noexcept = default;
    auto operator=(const Iterator_ &) noexcept -> Iterator_ & = default;
    auto operator=(Iterator_ &&) noexcept -> Iterator_ & = default;
    ~Iterator_() noexcept = default;

    template<bool IsConst_>
    friend class Iterator_;

    template<bool IsConst_, typename Requires_ = std::enable_if_t<!std::is_base_of_v<Iterator_, Iterator_<IsConst_>>>>
    Iterator_(const Iterator_<IsConst_> &that) noexcept : ptr_{that.ptr_} {
    }

    auto operator*() const noexcept -> const ValType & {
      return *ptr_;
    }
    auto operator->() const noexcept -> ValType * {
      return ptr_;
    }

    auto operator++() noexcept -> Iterator_ & {
      ++ptr_;
      return *this;
    }
    auto operator++(int) noexcept -> Iterator_ {
      Iterator_ tmp{*this};
      ++(*this);
      return tmp;
    }
    auto operator--() noexcept -> Iterator_ & {
      --ptr_;
      return *this;
    }
    auto operator--(int) noexcept -> Iterator_ {
      Iterator_ tmp{*this};
      --(*this);
      return tmp;
    }
    auto operator+=(difference_type n) noexcept -> Iterator_ & {
      ptr_ += n;
      return *this;
    }

    auto operator-=(difference_type n) noexcept -> Iterator_ & {
      ptr_ -= n;
      return *this;
    }

    auto operator[](std::ptrdiff_t n) const noexcept -> ValType & {
      return *(*this + n);
    }

    auto operator+(std::ptrdiff_t n) const noexcept -> Iterator_ {
      Iterator_ tmp{*this};
      tmp += n;
      return tmp;
    }

    auto operator-(std::ptrdiff_t n) const noexcept -> Iterator_ {
      Iterator_ tmp{*this};
      tmp -= n;
      return tmp;
    }

    template<bool IsConstA, bool IsConstB>
    friend auto operator==(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool;
    template<bool IsConstA, bool IsConstB>
    friend auto operator!=(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool;
    template<bool IsConstA, bool IsConstB>
    friend auto operator<(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool;
    template<bool IsConstA, bool IsConstB>
    friend auto operator>(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool;
    template<bool IsConstA, bool IsConstB>
    friend auto operator<=(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool;
    template<bool IsConstA, bool IsConstB>
    friend auto operator>=(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool;

    template<bool IsConstA, bool IsConstB>
    friend auto operator-(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> std::ptrdiff_t;
  };

  template<bool IsConstA, bool IsConstB>
  friend auto operator==(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool {
    return a.ptr_ == b.ptr_;
  }
  template<bool IsConstA, bool IsConstB>
  friend auto operator!=(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool {
    return a.ptr_ != b.ptr_;
  }

  template<bool IsConstA, bool IsConstB>
  friend auto operator<(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool {
    return a.ptr_ < b.ptr_;
  }

  template<bool IsConstA, bool IsConstB>
  friend auto operator>(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool {
    return a.ptr_ > b.ptr_;
  }

  template<bool IsConstA, bool IsConstB>
  friend auto operator<=(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool {
    return a.ptr_ <= b.ptr_;
  }
  template<bool IsConstA, bool IsConstB>
  friend auto operator>=(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> bool {
    return a.ptr_ >= b.ptr_;
  }

  template<bool IsConstA, bool IsConstB>
  friend auto operator-(const Iterator_<IsConstA> &a, const Iterator_<IsConstB> &b) -> std::ptrdiff_t {
    return a.ptr_ - b.ptr_;
  }

private:
  auto alloc_heap_(std::uint64_t capacity) -> ValType * {
    return allocator_traits_::allocate(alloc_, capacity);
  }

  void dealloc_heap_(ValType *heap_data, std::uint64_t capacity) {
    allocator_traits_::deallocate(alloc_, heap_data, capacity);
  }

  void destruct_in_(ValType *pos, std::uint64_t size) noexcept(noexcept(std::is_nothrow_destructible_v<ValType>)) {
    if constexpr (!std::is_trivially_destructible_v<ValType>) {
      for (std::ptrdiff_t i = 0; i < size; i++) {
        allocator_traits_::destroy(alloc_, pos++);
      }
    }
  }

  void uninitialized_copy_elements_(ValType *dst, const ValType *src,
                                    std::uint64_t size) noexcept(std::is_nothrow_copy_constructible_v<ValType> ||
                                                                 std::is_nothrow_move_constructible_v<ValType>) {
    if constexpr (std::is_trivially_copyable_v<ValType>) {
      std::memcpy(dst, src, sizeof(ValType) * size);
    } else {
      if constexpr (std::is_nothrow_copy_constructible_v<ValType>) {
        for (std::ptrdiff_t i = 0; i < size; i++) {
          allocator_traits_::construct(alloc_, dst++, src[i]);
        }
      } else {
        std::ptrdiff_t i{};
        ValType *tmp{alloc_heap_(size)};
        ValType *tmp_cur{};
        try {
          for (tmp_cur = tmp, i = 0; i < size; tmp_cur++, i++) {
            allocator_traits_::construct(alloc_, tmp_cur, src[i]);
          }
          for (tmp_cur = tmp, i = 0; i < size; tmp_cur++, i++, dst++) {
            allocator_traits_::construct(alloc_, dst, std::move(*tmp_cur));
          }
          destruct_in_(tmp, size);
          dealloc_heap_(tmp, size);
        } catch (...) {
          destruct_in_(tmp, i);
          dealloc_heap_(tmp, size);
          throw;
        }
      }
    }
  }

  void copy_elements_(ValType *dst, const ValType *src,
                      std::uint64_t size) noexcept(std::is_nothrow_copy_assignable_v<ValType> ||
                                                   std::is_nothrow_move_assignable_v<ValType>) {
    if constexpr (std::is_trivially_copyable_v<ValType>) {
      std::memcpy(dst, src, sizeof(ValType) * size);
    } else {
      if constexpr (std::is_nothrow_copy_assignable_v<ValType>) {
        for (std::ptrdiff_t i = 0; i < size; i++) {
          *dst++ = src[i];
        }
      } else {
        std::ptrdiff_t i{};
        ValType *tmp{alloc_heap_(size)};
        ValType *tmp_cur{};
        try {
          for (tmp_cur = tmp, i = 0; i < size; tmp_cur++, i++) {
            allocator_traits_::construct(alloc_, tmp_cur, src[i]);
          }
          for (tmp_cur = tmp, i = 0; i < size; tmp_cur++, i++, dst++) {
            allocator_traits_::construct(alloc_, dst, std::move(*tmp_cur));
          }
          destruct_in_(tmp, size);
          dealloc_heap_(tmp, size);
        } catch (...) {
          destruct_in_(tmp, i);
          dealloc_heap_(tmp, size);
          throw;
        }
      }
    }
  }

  void uninitialized_move_elements_(ValType *dst, ValType *src,
                                    std::uint64_t size) noexcept(std::is_nothrow_move_constructible_v<ValType> ||
                                                                 std::is_nothrow_copy_constructible_v<ValType>) {
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

  void move_elements_(ValType *dst, ValType *src,
                      std::uint64_t size) noexcept(std::is_nothrow_move_assignable_v<ValType> ||
                                                   std::is_nothrow_copy_assignable_v<ValType>) {
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

  void expand_with_(std::ptrdiff_t offset, std::uint64_t with_size, std::uint64_t incre_factor = INCREMENT_FACTOR) {
    std::uint64_t needed_capacity{size_ + with_size};
    offset *= sizeof(ValType);
    with_size *= sizeof(ValType);
    if (needed_capacity <= capacity_) [[likely]] {
      bool is_overlapped{with_size < size_ - offset};
      if (!is_overlapped) [[likely]] {
        uninitialized_move_elements_(data_pos_ + offset + with_size, data_pos_ + offset, size_ - offset);
      } else {
        uninitialized_move_elements_(data_pos_ + size_, data_pos_ + size_ - with_size, with_size);
        move_elements_(data_pos_ + offset + with_size, data_pos_ + offset, size_ - offset - with_size);
      }
      destruct_in_(data_pos_ + offset, with_size);
    } else {
      std::uint64_t new_capacity{std::max(size_ * incre_factor, needed_capacity)};
      ValType *new_heap_data{alloc_heap_(new_capacity)};
      uninitialized_move_elements_(new_heap_data, data_pos_, offset);
      uninitialized_move_elements_(new_heap_data + offset + with_size, data_pos_ + offset, size_ - offset);
      destruct_in_(data_pos_, size_);
      if (!is_inline_) {
        dealloc_heap_(data_pos_, capacity_);
      }
      is_inline_ = false;
      capacity_ = new_capacity;
      data_pos_ = new_heap_data;
    }
  }

  void shrink_without_(std::ptrdiff_t offset, std::uint64_t without_size) {
    std::uint64_t needed_capacity{size_ - without_size};
    ValType *inline_data{reinterpret_cast<ValType *>(inline_data_)};
    if (is_inline_) [[likely]] {
      move_elements_(inline_data + offset, inline_data + offset + without_size, size_ - (offset + without_size));
      destruct_in_(inline_data + size_ - without_size, without_size);
    } else {
      if (needed_capacity > BufferSize) [[likely]] {
        move_elements_(data_pos_ + offset, data_pos_ + offset + without_size, size_ - (offset + without_size));
        destruct_in_(data_pos_ + size_ - without_size, without_size);
      } else {
        uninitialized_move_elements_(inline_data, data_pos_, offset);
        uninitialized_move_elements_(inline_data + offset, data_pos_ + offset + without_size,
                                     size_ - (offset + without_size));
        destruct_in_(data_pos_, size_);
        dealloc_heap_(data_pos_, capacity_);
        is_inline_ = true;
        capacity_ = BufferSize;
        data_pos_ = inline_data;
      }
    }
  }

  auto visit_cached_ptr() const noexcept -> ValType * {
    return std::launder(data_pos_);
  }

public:
  using value_type = ValType;
  using reference = ValType &;
  using iterator = Iterator_<false>;
  using const_iterator = Iterator_<true>;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

  using Iterator = Iterator_<false>;
  using ConstIterator = Iterator_<true>;
  using ReverseIterator = std::reverse_iterator<Iterator>;
  using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

  SmallVector() = default;

  SmallVector(const SmallVector &that) {
    alloc_ = allocator_traits_::select_on_container_copy_construction(that.alloc_);
    if (!that.is_inline_) [[unlikely]] {
      data_pos_ = alloc_heap_(that.capacity_);
    }
    uninitialized_copy_elements_(data_pos_, that.data_pos_, that.size_);
    size_ = that.size_;
    capacity_ = that.capacity_;
    is_inline_ = that.is_inline_;
  }

  SmallVector(SmallVector &&that) {
    alloc_ = std::move(that.alloc_);
    if (that.is_inline_) [[likely]] {
      ValType *inline_data{reinterpret_cast<ValType *>(inline_data_)};
      ValType *that_inline_data{reinterpret_cast<ValType *>(that.inline_data_)};
      uninitialized_move_elements_(inline_data, that_inline_data, that.size_);
    } else {
      data_pos_ = that.data_pos_;
      that.data_pos_ = nullptr;
    }
    size_ = that.size_;
    capacity_ = that.capacity_;
    is_inline_ = that.is_inline_;
  }

  auto operator=(const SmallVector &that) -> SmallVector & {
    if (this == &that) {
      return *this;
    }
    destruct_in_(data_pos_, size_);
    size_ = 0;

    if constexpr (allocator_traits_::propagate_on_container_copy_assignment::value) {
      if constexpr (!allocator_traits_::is_always_equal::value) {
        if (alloc_ != that.alloc_) {
          if (!is_inline_) [[unlikely]] {
            dealloc_heap_(data_pos_, capacity_);
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
      data_pos_ = alloc_heap_(that.capacity_);
      capacity_ = that.capacity_;
      is_inline_ = false;
    }
    uninitialized_copy_elements_(data_pos_, that.data_pos_, that.size_);
    size_ = that.size_;
    return *this;
  }

  auto operator=(SmallVector &&that) -> SmallVector & {
    if (this == &that) {
      return *this;
    }
    destruct_in_(data_pos_, size_);
    if constexpr (allocator_traits_::propagate_on_container_move_assignment::value) {
      if constexpr (!allocator_traits_::is_always_equal::value) {
        if (alloc_ != that.alloc_) {
          if (!is_inline_) [[unlikely]] {
            dealloc_heap_(data_pos_, capacity_);
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
      uninitialized_move_elements_(data_pos_, that.data_pos_, that.size_);
    } else {
      data_pos_ = that.data_pos_;
      that.data_pos_ = nullptr;
      capacity_ = that.capacity_;
      is_inline_ = false;
    }
    size_ = that.size_;
    return *this;
  }

  ~SmallVector() noexcept {
    if (data_pos_) [[likely]] {
      destruct_in_(data_pos_, size_);
      if (!is_inline_) [[unlikely]] {
        dealloc_heap_(data_pos_, capacity_);
      }
    }
  }

  auto operator[](std::ptrdiff_t index) const noexcept -> const ValType & {
    return data_pos_[index];
  }

  auto operator[](std::ptrdiff_t index) noexcept -> ValType & {
    return data_pos_[index];
  }

  auto begin() const noexcept -> ConstIterator {
    return data_pos_;
  }

  auto end() const noexcept -> ConstIterator {
    return data_pos_ + size_;
  }

  auto begin() noexcept -> Iterator {
    return data_pos_;
  }

  auto end() noexcept -> Iterator {
    return data_pos_ + size_;
  }

  auto cbegin() const noexcept -> ConstIterator {
    return data_pos_;
  }

  auto cend() const noexcept -> ConstIterator {
    return data_pos_ + size_;
  }

  auto rbegin() const noexcept -> ConstReverseIterator {
    return ConstReverseIterator{end()};
  }

  auto rend() const noexcept -> ConstReverseIterator {
    return ConstReverseIterator{begin()};
  }

  auto rbegin() noexcept -> ReverseIterator {
    return ReverseIterator{end()};
  }

  auto rend() noexcept -> ReverseIterator {
    return ReverseIterator{begin()};
  }

  auto crbegin() const noexcept -> ConstReverseIterator {
    return ConstReverseIterator{cend()};
  }

  auto crend() const noexcept -> ConstReverseIterator {
    return ConstReverseIterator{cbegin()};
  }

  auto size() const noexcept -> std::uint64_t {
    return size_;
  }

  auto max_size() const noexcept -> std::uint64_t {
    return std::numeric_limits<std::ptrdiff_t>::max() / sizeof(ValType);
  }

  auto capacity() const noexcept -> std::uint64_t {
    return capacity_;
  }

  auto back() const noexcept -> const ValType & {
    return data_pos_[size_ - 1];
  }

  auto back() noexcept -> ValType & {
    return data_pos_[size_ - 1];
  }

  auto front() const noexcept -> const ValType & {
    return data_pos_[0];
  }

  auto front() noexcept -> ValType & {
    return data_pos_[0];
  }

  template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
  auto insert(ConstIterator pos, ValType_ &&val) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    expand_with_(offset, 1, INCREMENT_FACTOR);
    allocator_traits_::construct(alloc_, std::addressof(data_pos_[offset]), std::forward<ValType_>(val));
    ++size_;
    return Iterator{std::addressof(data_pos_[offset])};
  }

  auto insert(ConstIterator pos, ConstIterator beg, ConstIterator end) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    std::ptrdiff_t len{end - beg};
    expand_with_(offset, len, INCREMENT_FACTOR);
    uninitialized_copy_elements_(std::addressof(data_pos_[offset]), std::addressof(*beg), len);
    size_ += len;
    return Iterator{std::addressof(data_pos_[offset])};
  }

  template<typename InputIter_,
           typename Requires_ = std::enable_if_t<!std::is_constructible_v<ConstIterator, InputIter_>>>
  auto insert(ConstIterator pos, InputIter_ beg, InputIter_ end) -> Iterator {
    using iter_categroy = typename std::iterator_traits<InputIter_>::iterator_category;
    std::ptrdiff_t offset{pos - cbegin()};
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, iter_categroy>) {
      std::ptrdiff_t len{end - beg};
      expand_with_(offset, len, INCREMENT_FACTOR);
      for (std::ptrdiff_t i = offset; beg != end; i++, beg++) {
        allocator_traits_::construct(alloc_, std::addressof(data_pos_[i]), *beg);
      }
      size_ += len;
    } else {
      SmallVector<ValType, BufferSize> tmp{};
      for (; beg != end; beg++) {
        tmp.emplace_back(*beg);
      }
      this->insert(pos, tmp.begin(), tmp.end());
    }
    return Iterator{std::addressof(data_pos_[offset])};
  }

  template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
  auto insert(ConstIterator pos, std::initializer_list<ValType_> list) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    expand_with_(offset, list.size(), INCREMENT_FACTOR);
    uninitialized_copy_elements_(std::addressof(data_pos_[offset]), list.begin(), list.size());
    size_ += list.size();
    return Iterator{std::addressof(data_pos_[offset])};
  }

  auto erase(ConstIterator pos) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    shrink_without_(offset, 1);
    --size_;
    return Iterator{std::addressof(data_pos_[offset])};
  }

  auto erase(ConstIterator beg, ConstIterator end) -> Iterator {
    std::ptrdiff_t offset{beg - cbegin()};
    std::ptrdiff_t len{end - beg};
    shrink_without_(offset, len);
    size_ -= len;
    return Iterator{std::addressof(data_pos_[offset])};
  }

  template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
  void push_back(ValType_ &&val) {
    if (size_ >= capacity_) [[unlikely]] {
      expand_with_(size_, 1, INCREMENT_FACTOR);
    }
    allocator_traits_::construct(alloc_, std::addressof(data_pos_[size_]), std::forward<ValType_>(val));
    ++size_;
  }

  template<typename... Args, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, Args...>>>
  auto emplace_back(Args &&...args) -> ValType & {
    if (size_ >= capacity_) [[unlikely]] {
      expand_with_(size_, 1, INCREMENT_FACTOR);
    }
    allocator_traits_::construct(alloc_, std::addressof(data_pos_[size_]), std::forward<Args>(args)...);
    return data_pos_[size_++];
  }

  void pop_back() {
    if (size_ == BufferSize + 1) [[unlikely]] {
      shrink_without_(size_ - 1, 1);
      return;
    }
    allocator_traits_::destroy(alloc_, std::addressof(data_pos_[size_]));
    --size_;
  }

  void resize(std::uint64_t size) {
    if (size > capacity_) [[likely]] {
      expand_with_(size_, size - size_);
    }
    for (std::ptrdiff_t i = size_; i < size; i++) {
      allocator_traits_::construct(alloc_, std::addressof(data_pos_[i]), ValType{});
    }
    size_ = size;
  }

  void reserve(std::uint64_t capacity) {
    if (capacity > capacity_) [[likely]] {
      expand_with_(size_, capacity - size_, 1);
    }
    capacity_ = capacity;
  }

  void clear() noexcept {
    shrink_without_(0, size_);
    size_ = 0;
  }
};

// ============================================================
//
// ============================================================

constexpr int OUTER = 10000000;
constexpr int INNER = 16;

template<class F>
long long time_ms(F &&f) {
  using clock = chrono::steady_clock;
  std::vector<std::thread> ts{};
  std::vector<decltype(clock::now())> start_times{};
  ts.resize(16);
  start_times.resize(16);
  std::barrier b{16};

  for (int i = 0; i < 16; i++) {
    ts[i] = std::thread{[&, i] {
      b.arrive_and_wait();
      start_times[i] = clock::now();
      f();
    }};
  }
  for (int i = 0; i < 16; i++) {
    ts[i].join();
  }

  auto end = clock::now();
  std::chrono::time_point<std::chrono::steady_clock> last_start_time{start_times[0]};
  for (int i = 0; i < 16; i++) {
    last_start_time = std::max(last_start_time, start_times[i]);
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - last_start_time).count();
}

void normal_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    std::vector<int> v;
    v.reserve(16);
    for (int j = 0; j < INNER; ++j) {
      v.insert(v.begin(), i + j * j);
      sum += v.front();
    }
  }

  cout << "[normal_test] sum= " << sum << endl;
}

void array_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    std::array<volatile int, INNER> arr{};
    for (int j = 0; j < INNER; ++j) {
      arr[j] = (i + j * j); // 用下标写入，等价于 vector 的 push_back
      sum += arr[j];
    }
  }

  cout << "[array_test] sum= " << sum << endl;
}

void pmr_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    alignas(std::max_align_t) std::byte buf[INNER * sizeof(int)];
    std::pmr::monotonic_buffer_resource mr(buf, sizeof(buf));
    std::pmr::vector<int> v{&mr};
    for (int j = 0; j < INNER; ++j) {
      v.emplace_back(i + j * j);
      sum += v.back();
    }
  }

  cout << "[pmr_test] sum= " << sum << endl;
}

void smallvec_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    SmallVector<int, INNER> arr{};
    for (int j = 0; j < INNER; ++j) {
      arr.insert(arr.end(), i + j * j);
      // arr.emplace_back(i + j * j);
      // sum += *(arr.end() - 1);
      sum += arr.back();
    }
  }

  cout << "[smallvec_test] sum= " << sum << endl;
}

int main() {

  // auto t1{time_ms([] { normal_test(); })};
  // std::cout << "[normal_test] time=" << t1 << " ms\n";
  // // // auto t2{time_ms([] { array_test(); })};
  // // // std::cout << "[array_test] time=" << t2 << " ms\n";
  // // // auto t3{time_ms([] { pmr_test(); })};
  // // // std::cout << "[pmr_test] time=" << t3 << " ms\n";
  // auto t4{time_ms([] { smallvec_test(); })};
  // std::cout << "[smallvec_test] time=" << t4 << " ms\n";

  // std::is_trivially_copyable_v<int>;
  int a1[0]{};
  int a2[0]{};
  cout << a1 << endl;
  cout << a2 << endl;
}