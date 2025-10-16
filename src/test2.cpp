#include <bits/stdc++.h>
using namespace std;

int main() {

  constexpr static int PAGE_SIZE{4096};
  auto ptr{new (std::align_val_t{64}) char[PAGE_SIZE]};

  ::operator delete[](ptr, PAGE_SIZE, std::align_val_t{64});
}