#include "SmallVector_formatted.h"
#include <bits/stdc++.h>
using namespace std;

// ============================================================
//
// ============================================================

constexpr int OUTER = 10000000;
constexpr int INNER = 16;

template<class F>
long long time_ms(F &&f, std::ptrdiff_t TCnt) {
  using clock = chrono::steady_clock;
  std::vector<std::thread> ts{};
  std::vector<decltype(clock::now())> start_times{};
  ts.resize(TCnt);
  start_times.resize(TCnt);
  std::barrier b{TCnt};

  for (int i = 0; i < TCnt; i++) {
    ts[i] = std::thread{[&, i] {
      b.arrive_and_wait();
      start_times[i] = clock::now();
      f();
    }};
  }
  for (int i = 0; i < TCnt; i++) {
    ts[i].join();
  }

  auto end = clock::now();
  std::chrono::time_point<std::chrono::steady_clock> last_start_time{start_times[0]};
  for (int i = 0; i < TCnt; i++) {
    last_start_time = std::max(last_start_time, start_times[i]);
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - last_start_time).count();
}

void normal_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    std::vector<int> v;
    v.reserve(16);
    for (int j = 0; j < INNER; ++j) {
      v.insert(v.begin(), i + j * j);
      sum += v.front();
    }
  }

  cout << "[normal_test] sum= " << sum << endl;
}

void array_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    std::array<volatile int, INNER> arr{};
    for (int j = 0; j < INNER; ++j) {
      arr[j] = (i + j * j); // 用下标写入，等价于 vector 的 push_back
      sum += arr[j];
    }
  }

  cout << "[array_test] sum= " << sum << endl;
}

void pmr_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    alignas(std::max_align_t) std::byte buf[INNER * sizeof(int)];
    std::pmr::monotonic_buffer_resource mr(buf, sizeof(buf));
    std::pmr::vector<int> v{&mr};
    for (int j = 0; j < INNER; ++j) {
      v.emplace_back(i + j * j);
      sum += v.back();
    }
  }

  cout << "[pmr_test] sum= " << sum << endl;
}

void smallvec_test() {
  long long sum = 0;

  for (int i = 0; i < OUTER; ++i) {
    meow_utils::SmallVector<int, INNER> arr{};
    for (int j = 0; j < INNER; ++j) {
      // arr.insert(arr.end(), i + j * j);
      arr.EmplaceBack(i + j * j);
      // sum += *(arr.end() - 1);
      sum += arr.Back();
    }
  }

  cout << "[smallvec_test] sum= " << sum << endl;
}

template<typename ArrType>
void concurrency_test() {

  std::vector<std::thread> ts(16);
  std::barrier b{16};

  constexpr int cnt{20000};
  std::vector<int> ex(cnt, 0);

  ArrType vec{};
  std::mutex m{};

  for (int i = 0; i < ts.size(); i++) {
    ts[i] = std::thread{[&]() {
      b.arrive_and_wait();
      for (int i = 0; i < cnt; i++) {
        std::lock_guard lk{m};
        if (i % 2 == 1) {
          vec.emplace_back(i);
        } else {
          vec.insert(vec.begin(), i);
        }
      }
      b.arrive_and_wait();

      while (true) {
        std::lock_guard lk{m};
        if (vec.size()) {
          ex[vec.back()]++;
          vec.pop_back();
        } else {
          break;
        }
      }
    }};
  }
  for (int i = 0; i < ts.size(); i++) {
    ts[i].join();
  }

  for (int i = 0; i < cnt; i++) {
    if (ex[i] != ts.size()) {
      cout << "FAILED" << endl;
      return;
    }
  }
  cout << "OK" << endl;
}

int main() {
  // auto t1{time_ms([] { normal_test(); }, 16)};
  // std::cout << "[normal_test] time=" << t1 << " ms\n";
  // // auto t2{time_ms([] { array_test(); })};
  // // std::cout << "[array_test] time=" << t2 << " ms\n";
  // // auto t3{time_ms([] { pmr_test(); })};
  // // std::cout << "[pmr_test] time=" << t3 << " ms\n";
  auto t4{time_ms([] { smallvec_test(); }, 16)};
  std::cout << "[smallvec_test] time=" << t4 << " ms\n";
}