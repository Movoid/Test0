#include "SimpleCU_QSBR.h"
#include <bits/stdc++.h>
using namespace std;

void time_count(auto x) {
  auto beg{std::chrono::high_resolution_clock::now()};
  x();
  auto end{std::chrono::high_resolution_clock::now()};
  cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - beg).count() << std::endl;
}

int main() {

  cout << "main start" << endl;

  struct deleter {
    void operator()(int id) {
      cout << id << " deleted" << endl;
    }
  };

  SimpleCU::QSBR::QSBRManager<20, int, deleter> mgr{deleter{}};
  cout << "manager inited" << endl;

  std::jthread t1{[&]() {
    cout << "t1 start" << endl;
    mgr.enter_critical_zone();
    mgr.retire(1);
    mgr.retire(2);
    mgr.retire(3);
    std::this_thread::sleep_for(std::chrono::seconds{5});
    mgr.exit_critical_zone();
    cout << "t1 quit" << endl;
  }};

  std::jthread t2{[&]() {
    cout << "t2 start" << endl;
    mgr.enter_critical_zone();
    mgr.retire(6);
    mgr.retire(7);
    mgr.retire(8);
    mgr.exit_critical_zone();
    cout << "t2 quit" << endl;
  }};
  t2.join();

  mgr.enter_critical_zone();
  mgr.retire(-1);
  mgr.retire(-2);
  mgr.retire(-3);

  t1.join();
  mgr.exit_critical_zone();
  cout << "main quit" << endl;
}