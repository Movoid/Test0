#include <bits/stdc++.h>
using namespace std;

template<typename DeleterType, typename Requires = void>
class EBODeleterStorage {
private:
  DeleterType deleter_;

public:
  EBODeleterStorage(const DeleterType &deleter) : deleter_{deleter} {
  }
  EBODeleterStorage(const EBODeleterStorage &) = default;
  EBODeleterStorage(EBODeleterStorage &&) = default;
  auto operator=(const EBODeleterStorage &) -> EBODeleterStorage & = default;
  auto operator=(EBODeleterStorage &&) -> EBODeleterStorage & = default;
  ~EBODeleterStorage() = default;

  auto get_deleter() -> DeleterType & {
    return deleter_;
  }
};

template<typename DeleterType>
class EBODeleterStorage<DeleterType, std::enable_if_t<std::is_empty_v<DeleterType>>> : public DeleterType {
public:
  EBODeleterStorage() = default;
  EBODeleterStorage(const EBODeleterStorage &) = default;
  EBODeleterStorage(EBODeleterStorage &&) = default;
  auto operator=(const EBODeleterStorage &) -> EBODeleterStorage & = default;
  auto operator=(EBODeleterStorage &&) -> EBODeleterStorage & = default;
  ~EBODeleterStorage() = default;

  auto get_deleter() -> DeleterType & {
    return *this;
  }
};

class Manager {
private:
  struct RetiredNodeConcept {
    RetiredNodeConcept() = default;
    virtual ~RetiredNodeConcept() = default;
    virtual void delete_this() = 0;
  };

  // EBO 有效, 最小 16bytes, 其中 vptr 占 8bytes.
  template<typename ValType, typename DeleterType>
  struct RetiredNode : EBODeleterStorage<DeleterType>, RetiredNodeConcept {
    ValType val_;

    RetiredNode(ValType &&val) : EBODeleterStorage<DeleterType>{}, val_{val} {
    }

    RetiredNode(ValType &&val, const DeleterType &deleter) : EBODeleterStorage<DeleterType>{deleter}, val_{val} {
    }

    void delete_this() override {
      this->get_deleter()(val_);
    }
  };
  std::vector<std::unique_ptr<RetiredNodeConcept>> vec_;

public:
  template<typename ValType, typename DeleterType = std::default_delete<std::remove_pointer_t<ValType>>>
  void retire(ValType &&val) { // move
    cout << "this node type size " << sizeof(RetiredNode<ValType, DeleterType>) << endl;
    vec_.emplace_back(std::make_unique<RetiredNode<ValType, DeleterType>>(std::move(val)));
  }
  void delete_all() {
    for (auto &node : vec_) {
      node->delete_this();
    }
  }
};

class A {};

class B {};

class C : A, B {};

int main() {

  cout << sizeof(C) << endl;

  Manager m{};
  int *a{new int{1}};
  double *b{new double{1.5}};

  struct double_deleter {
    void operator()(double *ptr) {
      cout << "double deleter " << *ptr << endl;
      delete ptr;
    }
  };

  m.retire<int *>(std::move(a));
  m.retire<double *, double_deleter>(std::move(b));

  m.delete_all();
}