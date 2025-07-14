#include <bits/stdc++.h>

volatile int data{};
volatile int flag{};

void p1() {
  data = 666;
  asm(R"(
    mfence
  )"
      :
      :
      : "memory");
  flag = true;
}

void p2() {
  while (!flag) {
    asm(R"(
      mfence
    )"
        :
        :
        : "memory"); // not neccessary in x86
    printf("%d\n", data);
  }
}

int main() {
}