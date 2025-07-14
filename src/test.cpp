#include <bits/stdc++.h>
using namespace std;

int val1{};
std::atomic<int> val2{};

void p0() {
  val1 = 1;
  val2.store(2, std::memory_order_relaxed);
}

void p1() {
  while (val2.load(std::memory_order_relaxed) != 2) {
    ;
  }
  cout << val1 << endl;
}

int main() {
  std::jthread t1{p0};
  std::jthread t2{p1};
}