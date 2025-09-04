#include "SimpleCU_QSBR.h"
#include <bits/stdc++.h>
using namespace std;

int main() {

  struct id_deleter {
    int status{};
    void operator()(int id) const {
      std::print("status {}, id {}\n", status, id);
    }
  };

  const id_deleter del1{1};

  SimpleCU::QSBR::QSBRManager<10, int, const id_deleter> m{del1};

  m.enter_critical_zone();

  m.retire(1);

  m.exit_critical_zone();

  m.reclaim_local();
}
