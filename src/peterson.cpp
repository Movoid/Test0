#include <bits/stdc++.h>
using namespace std;

volatile bool flag[2]{};
volatile int sum{};
volatile int turn{};

std::barrier b1{2}; // start from the same time point

void p0() {
  b1.arrive_and_wait();
  flag[0] = true;
  turn = 1;
  asm(R"(
    mfence
  )"
      :
      :
      : "memory");
  while (flag[1] == true && turn == 1)
    ; // spin
  for (int i = 0; i < 10000; i++) {
    sum++;
  }
  flag[0] = false;
}

void p1() {
  b1.arrive_and_wait();
  flag[1] = true;
  turn = 0;
  asm(R"(
    mfence
  )"
      :
      :
      : "memory");
  while (flag[0] == true && turn == 0)
    ; // spin
  for (int i = 0; i < 10000; i++) {
    sum++;
  }
  flag[1] = false;
}

int main() {
  for (int i = 0; i < 10000; i++) {
    std::jthread j1{p0};
    std::jthread j2{p1};
  }

  cout << sum << endl;
}
