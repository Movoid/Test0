#include <bits/stdc++.h>
using namespace std;

template<typename ValType, std::uint64_t BufferSize, typename Alloc = std::allocator<ValType>>
class SmallVector {
private:
  ValType inline_data_[BufferSize];
  ValType *data_pos_{inline_data_};
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

    auto operator*() const -> const ValType & {
      return *ptr_;
    }
    auto operator->() const -> ValType * {
      return ptr_;
    }

    auto operator++() -> Iterator_ & {
      ++ptr_;
      return *this;
    }
    auto operator++(int) -> Iterator_ {
      Iterator_ tmp{*this};
      ++(*this);
      return tmp;
    }
    auto operator--() -> Iterator_ & {
      --ptr_;
      return *this;
    }
    auto operator--(int) -> Iterator_ {
      Iterator_ tmp{*this};
      --(*this);
      return tmp;
    }
    auto operator+=(difference_type n) -> Iterator_ & {
      ptr_ += n;
      return *this;
    }

    auto operator-=(difference_type n) -> Iterator_ & {
      ptr_ -= n;
      return *this;
    }

    auto operator[](std::ptrdiff_t n) const -> ValType & {
      return *(*this + n);
    }

    auto operator+(std::ptrdiff_t n) const -> Iterator_ {
      Iterator_ tmp{*this};
      tmp += n;
      return tmp;
    }

    auto operator-(std::ptrdiff_t n) const -> Iterator_ {
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
  auto alloc_heap(std::uint64_t capacity) -> ValType * {
    return allocator_traits_::allocate(alloc_, capacity);
  }

  void dealloc_heap(ValType *heap_data, std::uint64_t capacity) {
    allocator_traits_::deallocate(alloc_, heap_data, capacity);
  }

  void destruct_in(ValType *pos, std::uint64_t size) {
    if constexpr (!std::is_trivially_destructible_v<ValType>) {
      for (std::ptrdiff_t i = 0; i < size; i++) {
        allocator_traits_::destroy(alloc_, pos + i);
      }
    }
  }

  void uninitialized_copy_elements(ValType *dst, const ValType *src, std::uint64_t size) {
    if constexpr (std::is_trivially_copyable_v<ValType>) {
      std::memcpy(dst, src, sizeof(ValType) * size);
    } else {
      for (std::ptrdiff_t i = 0; i < size; i++) {
        allocator_traits_::construct(alloc_, std::addressof(dst[i]), src[i]);
      }
    }
  }

  void copy_elements(ValType *dst, const ValType *src, std::uint64_t size) {
    if constexpr (std::is_trivially_copyable_v<ValType>) {
      std::memcpy(dst, src, sizeof(ValType) * size);
    } else {
      for (std::ptrdiff_t i = 0; i < size; i++) {
        dst[i] = src[i];
      }
    }
  }

  void uninitialized_move_elements(ValType *dst, ValType *src, std::uint64_t size) {
    if constexpr (std::is_trivially_copyable_v<ValType>) {
      std::memmove(dst, src, sizeof(ValType) * size);
    } else {
      for (std::uint64_t i = 0; i < size; i++) {
        allocator_traits_::construct(alloc_, std::addressof(dst[i]), std::move(src[i]));
      }
    }
  }

  void move_elements(ValType *dst, ValType *src, std::uint64_t size) {
    if constexpr (std::is_trivially_copyable_v<ValType>) {
      std::memmove(dst, src, sizeof(ValType) * size);
    } else {
      for (std::uint64_t i = 0; i < size; i++) {
        dst[i] = std::move(src[i]);
      }
    }
  }

  void expand_with(std::ptrdiff_t offset, std::uint64_t with_size, std::uint64_t incre_factor) {
    std::uint64_t needed_capacity{size_ + with_size};
    if (needed_capacity <= capacity_) [[likely]] {
      bool is_overlapped{with_size < size_ - offset};
      if (!is_overlapped) [[likely]] {
        uninitialized_move_elements(data_pos_ + offset + with_size, data_pos_ + offset, size_ - offset);
      } else {
        uninitialized_move_elements(data_pos_ + size_, data_pos_ + size_ - with_size, with_size);
        move_elements(data_pos_ + offset + with_size, data_pos_ + offset, size_ - offset - with_size);
      }
      destruct_in(data_pos_ + offset, with_size);
    } else {
      std::uint64_t new_capacity{std::max(size_ * incre_factor, needed_capacity)};
      ValType *new_heap_data{alloc_heap(new_capacity)};
      uninitialized_move_elements(new_heap_data, data_pos_, offset);
      uninitialized_move_elements(new_heap_data + offset + with_size, data_pos_ + offset, size_ - offset);
      destruct_in(data_pos_, size_);
      if (!is_inline_) {
        dealloc_heap(data_pos_, capacity_);
      }
      is_inline_ = false;
      capacity_ = new_capacity;
      data_pos_ = new_heap_data;
    }
  }

  void shrink_without(std::ptrdiff_t offset, std::uint64_t without_size) {
    std::uint64_t needed_capacity{size_ - without_size};
    if (is_inline_) [[likely]] {
      move_elements(inline_data_ + offset, inline_data_ + offset + without_size, size_ - (offset + without_size));
      destruct_in(inline_data_ + size_ - without_size, without_size);
    } else {
      if (needed_capacity > BufferSize) [[likely]] {
        move_elements(data_pos_ + offset, data_pos_ + offset + without_size, size_ - (offset + without_size));
        destruct_in(data_pos_ + size_ - without_size, without_size);
      } else {
        uninitialized_move_elements(inline_data_, data_pos_, offset);
        uninitialized_move_elements(inline_data_ + offset, data_pos_ + offset + without_size,
                                    size_ - (offset + without_size));
        destruct_in(data_pos_, size_);
        dealloc_heap(data_pos_, capacity_);
        is_inline_ = true;
        capacity_ = BufferSize;
        data_pos_ = inline_data_;
      }
    }
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
      data_pos_ = alloc_heap(that.capacity_);
    }
    uninitialized_copy_elements(data_pos_, that.data_pos_, that.size_);
    size_ = that.size_;
    capacity_ = that.capacity_;
    is_inline_ = that.is_inline_;
  }

  SmallVector(SmallVector &&that) {
    alloc_ = std::move(that.alloc_);
    if (that.is_inline_) [[likely]] {
      uninitialized_move_elements(inline_data_, that.inline_data_, that.size_);
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
    destruct_in(data_pos_, size_);
    size_ = 0;

    if constexpr (allocator_traits_::propagate_on_container_copy_assignment::value) {
      if constexpr (!allocator_traits_::is_always_equal::value) {
        if (alloc_ != that.alloc_) {
          if (!is_inline_) [[unlikely]] {
            dealloc_heap(data_pos_, capacity_);
            data_pos_ = inline_data_;
            capacity_ = BufferSize;
            is_inline_ = true;
          }
          alloc_ = that.alloc_;
        }
      }
    }

    if (that.size_ > capacity_) [[unlikely]] {
      data_pos_ = alloc_heap(that.capacity_);
      capacity_ = that.capacity_;
      is_inline_ = false;
    }
    uninitialized_copy_elements(data_pos_, that.data_pos_, that.size_);
    size_ = that.size_;
    return *this;
  }

  auto operator=(SmallVector &&that) -> SmallVector & {
    if (this == &that) {
      return *this;
    }
    destruct_in(data_pos_, size_);
    if constexpr (allocator_traits_::propagate_on_container_move_assignment::value) {
      if constexpr (!allocator_traits_::is_always_equal::value) {
        if (alloc_ != that.alloc_) {
          if (!is_inline_) [[unlikely]] {
            dealloc_heap(data_pos_, capacity_);
            data_pos_ = inline_data_;
            capacity_ = BufferSize;
            is_inline_ = true;
          }
          alloc_ = std::move(that.alloc_);
        }
      }
    }

    if (that.is_inline_) [[likely]] {
      uninitialized_move_elements(data_pos_, that.data_pos_, that.size_);
    } else {
      data_pos_ = that.data_pos_;
      that.data_pos_ = nullptr;
      capacity_ = that.capacity_;
      is_inline_ = false;
    }
    size_ = that.size_;
    return *this;
  }

  ~SmallVector() {
    if (data_pos_) [[likely]] {
      destruct_in(data_pos_, size_);
      if (!is_inline_) [[unlikely]] {
        dealloc_heap(data_pos_, capacity_);
      }
    }
  }

  auto operator[](std::ptrdiff_t index) const -> const ValType & {
    return data_pos_[index];
  }

  auto operator[](std::ptrdiff_t index) -> ValType & {
    return data_pos_[index];
  }

  auto begin() const -> ConstIterator {
    return data_pos_;
  }

  auto end() const -> ConstIterator {
    return data_pos_ + size_;
  }

  auto begin() -> Iterator {
    return data_pos_;
  }

  auto end() -> Iterator {
    return data_pos_ + size_;
  }

  auto cbegin() const -> ConstIterator {
    return data_pos_;
  }

  auto cend() const -> ConstIterator {
    return data_pos_ + size_;
  }

  auto rbegin() const -> ConstReverseIterator {
    return ConstReverseIterator{end()};
  }

  auto rend() const -> ConstReverseIterator {
    return ConstReverseIterator{begin()};
  }

  auto rbegin() -> ReverseIterator {
    return ReverseIterator{end()};
  }

  auto rend() -> ReverseIterator {
    return ReverseIterator{begin()};
  }

  auto crbegin() const -> ConstReverseIterator {
    return ConstReverseIterator{cend()};
  }

  auto crend() const -> ConstReverseIterator {
    return ConstReverseIterator{cbegin()};
  }

  auto size() const -> std::uint64_t {
    return size_;
  }

  auto max_size() const -> std::uint64_t {
    return std::numeric_limits<std::ptrdiff_t>::max() / sizeof(ValType);
  }

  auto capacity() const -> std::uint64_t {
    return capacity_;
  }

  auto back() const -> const ValType & {
    return data_pos_[size_ - 1];
  }

  auto back() -> ValType & {
    return data_pos_[size_ - 1];
  }

  auto front() const -> const ValType & {
    return data_pos_[0];
  }

  auto front() -> ValType & {
    return data_pos_[0];
  }

  template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
  auto insert(ConstIterator pos, ValType_ &&val) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    expand_with(offset, 1, INCREMENT_FACTOR);
    allocator_traits_::construct(alloc_, std::addressof(data_pos_[offset]), std::forward<ValType_>(val));
    ++size_;
    return Iterator{std::addressof(data_pos_[offset])};
  }

  auto insert(ConstIterator pos, ConstIterator beg, ConstIterator end) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    std::ptrdiff_t len{end - beg};
    expand_with(offset, len, INCREMENT_FACTOR);
    uninitialized_copy_elements(std::addressof(data_pos_[offset]), std::addressof(*beg), len);
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
      expand_with(offset, len, INCREMENT_FACTOR);
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

  auto erase(ConstIterator pos) -> Iterator {
    std::ptrdiff_t offset{pos - cbegin()};
    shrink_without(offset, 1);
    --size_;
    return Iterator{std::addressof(data_pos_[offset])};
  }

  template<typename ValType_, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, ValType_>>>
  void push_back(ValType_ &&val) {
    if (size_ >= capacity_) [[unlikely]] {
      expand_with(size_, 1);
    }
    allocator_traits_::construct(alloc_, std::addressof(data_pos_[size_]), std::forward<ValType_>(val));
    ++size_;
  }

  template<typename... Args, typename Requires_ = std::enable_if_t<std::is_constructible_v<ValType, Args...>>>
  auto emplace_back(Args &&...args) -> ValType & {
    if (size_ >= capacity_) [[unlikely]] {
      expand_with(size_, 1, INCREMENT_FACTOR);
    }
    allocator_traits_::construct(alloc_, std::addressof(data_pos_[size_]), std::forward<Args>(args)...);
    return data_pos_[size_++];
  }

  void pop_back() {
    if (size_ == BufferSize + 1) [[unlikely]] {
      shrink_without(size_, 1);
      return;
    }
    allocator_traits_::destruct(alloc_, std::addressof(data_pos_[size_]));
    --size_;
  }

  void resize(std::uint64_t size) {
    if (size > capacity_) [[likely]] {
      expand_with(size_, size - size_);
    }
    for (std::ptrdiff_t i = size_; i < size; i++) {
      allocator_traits_::construct(alloc_, std::addressof(data_pos_[i]), ValType{});
    }
    size_ = size;
  }

  void reserve(std::uint64_t capacity) {
    if (capacity > capacity_) [[likely]] {
      expand_with(size_, capacity - size_, 1);
    }
    capacity_ = capacity;
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
    for (int j = 0; j < INNER; ++j) {
      v.reserve(16);
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
  alignas(std::max_align_t) std::byte buf[INNER * sizeof(int)];
  std::pmr::monotonic_buffer_resource mr(buf, sizeof(buf));

  for (int i = 0; i < OUTER; ++i) {
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
      // arr.insert(arr.end(), i + j * j);
      arr.emplace_back(i + j * j);
      // sum += *(arr.end() - 1);
      sum += arr.back();
    }
  }

  cout << "[smallvec_test] sum= " << sum << endl;
}

int main() {
  auto t1{time_ms([] { normal_test(); })};
  std::cout << "[normal_test] time=" << t1 << " ms\n";
  auto t2{time_ms([] { array_test(); })};
  std::cout << "[array_test] time=" << t2 << " ms\n";
  // auto t3{time_ms([] { pmr_test(); })};
  // std::cout << "[pmr_test] time=" << t3 << " ms\n";
  auto t4{time_ms([] { smallvec_test(); })};
  std::cout << "[smallvec_test] time=" << t4 << " ms\n";

  SmallVector<int, 4> sv{};

  std::deque<int> dq{1, 2, 3, 4, 5, 6, 7, 8};

  sv.insert(sv.begin(), dq.begin(), dq.end());

  std::println("{}", dq);
  std::println("{}", sv);

  return 0;
}