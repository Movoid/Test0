#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory_resource>
#include <vector>

using namespace std;

template<typename IdType>
class ABase {};

template<std::size_t Id>
class A : private ABase<std::size_t> {
private:
  std::size_t id_{Id};

public:
  A() = default;
  A(const A &) = default;

  template<typename T>
  struct A_traits : A_traits<std::decay_t<T>> {};

  template<std::size_t Id_>
  struct A_traits<A<Id_>> {
    constexpr static std::size_t id{Id_};
  };

  template<std::size_t Id_>
  friend class A;

  // (1)
  template<typename That_, typename Requires_ = std::enable_if_t<!std::is_base_of_v<A, std::decay_t<That_>> &&
                                                                 std::is_base_of_v<ABase, std::decay_t<That_>>>>
  A(That_ &&that) : id_{std::forward<That_>(that).id_} {
    cout << "from id " << A_traits<That_>::id << endl;
  }

  void say() {
    cout << id_ << endl;
  }
};

int main() {
  // A<3> a3{};
  // A<4> a4{a3};
  // a4.say();
  // int xxx{5};
}