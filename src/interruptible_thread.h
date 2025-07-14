#include <bits/stdc++.h>
using namespace std;

class thread_interrupted : public std::exception {};

void interruption_point();

class interrupt_flag {
  std::atomic<bool> flag;
  std::condition_variable *thread_cond;
  std::condition_variable_any *thread_cond_any;
  std::mutex set_clear_mutex;

public:
  interrupt_flag() : thread_cond(0), thread_cond_any(0) {
  }
  void set() {
    flag.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lk(set_clear_mutex);
    if (thread_cond) {
      thread_cond->notify_all();
    } else if (thread_cond_any) {
      thread_cond_any->notify_all();
    }
  }
  bool is_set() const {
    return flag.load();
  }
  template<typename Lockable>
  void wait(std::condition_variable_any &cv, Lockable &lk) {
    struct custom_lock {
      interrupt_flag *self;
      Lockable &lk;
      custom_lock(interrupt_flag *self_, std::condition_variable_any &cond, Lockable &lk_) : self(self_), lk(lk_) {
        self->set_clear_mutex.lock();

        self->thread_cond_any = &cond;
      }
      void unlock() {
        lk.unlock();
        self->set_clear_mutex.unlock();
      }
      void lock() {
        std::lock(self->set_clear_mutex, lk);
      }
      ~custom_lock() {
        self->thread_cond_any = 0;
        self->set_clear_mutex.unlock();
      }
    };
    custom_lock cl(this, cv, lk);
    interruption_point();
    cv.wait(cl);
    interruption_point();
  }
};

thread_local interrupt_flag this_thread_interrupt_flag;

void interruption_point() {
  if (this_thread_interrupt_flag.is_set()) {
    throw thread_interrupted{};
  }
}

template<typename Lockable>
void interruptible_wait(std::condition_variable_any &cv, Lockable &lk) {
  this_thread_interrupt_flag.wait(cv, lk);
}

class interruptible_htread {
private:
  std::thread internal_thread_;
  interrupt_flag *flag_;

public:
  template<typename FuncType>
  interruptible_htread(FuncType &&f) {
    std::promise<interrupt_flag *> p{};
    internal_thread_ = std::thread([f_ = std::forward<FuncType>(f), &p]() {
      p.set_value(&this_thread_interrupt_flag);
      f_();
    });
    flag_ = p.get_future().get();
  }

  void interrupt() {
    if (flag_) {
      flag_->set();
    }
  }
};