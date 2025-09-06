#include <bits/stdc++.h>
using namespace std;

class Obj {};

void foo(const Obj &obj) {
  cout << "const &" << endl;
}

void foo(Obj &&obj) {
  cout << "&&" << endl;
}

int main() {

  auto func1{[](auto &&obj) { foo(std::forward<decltype(obj)>(obj)); }};

  Obj a{};
  Obj b{};
  func1(a);
  func1(std::move(b));
}