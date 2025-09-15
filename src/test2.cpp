#include <bits/stdc++.h>
using namespace std;

class A {
private:
public:
  A() = default;
  A(const A &) = delete;
  auto operator=(const A &) -> A & = delete;
  A(A &&) = default;
  auto operator=(A &&) -> A & = default;
};

template<bool Value_>
struct use_alloc {};

template<bool Value_>
struct destruct_before {};

template<bool option_a>
void option(use_alloc<option_a> a) {
  cout << option_a << endl;
}

int main() {

  std::cout << (std::is_trivially_copy_constructible_v<A>) << std::endl;
  std::cout << (std::is_trivially_move_constructible_v<A>) << std::endl;
  std::cout << (std::is_trivially_copyable_v<A>) << std::endl;

  std::vector<int> arr1{};
  arr1.reserve(100);
  std::vector<int> arr2{arr1};
  cout << arr2.capacity() << endl;
}