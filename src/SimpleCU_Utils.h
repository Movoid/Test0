#pragma once
#include <bits/stdc++.h>

namespace SimpleCU::Utils {

  constexpr static std::size_t ALIGNMENT{std::hardware_constructive_interference_size};

  /**
   * WARNING: Never use this class with polymorphism.
   * The base class may not have a `virtual` destructor.
   */
  template<typename ValType, typename Requires = void>
  struct Aligned {};

  template<typename ValType>
  struct alignas(ALIGNMENT) Aligned<ValType, std::enable_if_t<std::is_class_v<ValType>>> : public ValType {
    using ValType::ValType;
    using ValType::operator=;

    template<typename ValType_,
             typename Requires_ = std::enable_if_t<std::is_same_v<std::decay_t<ValType>, std::decay_t<ValType_>>>>
    Aligned(ValType_ &&that) : ValType(std::forward<ValType_>(that)) {
    }
  };

  template<typename ValType>
  struct alignas(ALIGNMENT) Aligned<ValType, std::enable_if_t<!std::is_class_v<ValType>>> {
    ValType val_;

    explicit Aligned(const ValType &v) : val_(v) {
    }
    Aligned &operator=(const ValType &v) {
      val_ = v;
      return *this;
    }
    operator ValType &() {
      return val_;
    }
    operator const ValType &() const {
      return val_;
    }
  };

  template<typename ValType>
  using DefaultDeleter = std::default_delete<std::remove_pointer_t<ValType>>;

  template<typename DeleterType, typename Requires = void>
  class EBODeleterStorage {
  private:
    DeleterType deleter_;

  public:
    EBODeleterStorage() = default;                                      // 如果非空 DeleterType 能空参构造, 则也允许
    EBODeleterStorage(const DeleterType &deleter) : deleter_{deleter} { // 使用一个 DeleterType 对象构造
    }
    EBODeleterStorage(const EBODeleterStorage &) = default;
    EBODeleterStorage(EBODeleterStorage &&) = default;
    auto operator=(const EBODeleterStorage &) -> EBODeleterStorage & = default;
    auto operator=(EBODeleterStorage &&) -> EBODeleterStorage & = default;
    ~EBODeleterStorage() = default;

    auto get_deleter() -> DeleterType & {
      return deleter_;
    }
  };

  template<typename DeleterType>
  class EBODeleterStorage<DeleterType, std::enable_if_t<std::is_empty_v<DeleterType>>> : public DeleterType {
  public:
    EBODeleterStorage() = default;                                         // 空的 DeleterType 通常可以空参构造
    EBODeleterStorage(const DeleterType &deleter) : DeleterType{deleter} { // 也允许使用一个已经构造了的空 DeleterType.
    }
    EBODeleterStorage(const EBODeleterStorage &) = default;
    EBODeleterStorage(EBODeleterStorage &&) = default;
    auto operator=(const EBODeleterStorage &) -> EBODeleterStorage & = default;
    auto operator=(EBODeleterStorage &&) -> EBODeleterStorage & = default;
    ~EBODeleterStorage() = default;

    auto get_deleter() -> DeleterType & {
      return *this;
    }
  };
} // namespace SimpleCU::Utils