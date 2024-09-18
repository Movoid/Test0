#include <bits/stdc++.h>
#include "config.h"
#include "mymath.h"
using namespace std;

template<typename Deri>
class Base {
public:
    template<typename... Args>
    void interface(Args&&... args) {
        static_cast<Deri*>(this)->impl(std::forward<Args>(args)...);
    }

};

class A : public Base<A> {
public:
    void impl(int a, int b) {
        cout << "class A impl" << endl;
        cout << a + b << endl;
    }
};

class B : public Base<B> {
public:
    void impl(int a, int b) {
        cout << "class B impl" << endl;
        cout << a - b << endl;
    }
};

class Test {

};


// some traits
template<typename T, typename Requires = void>
struct is_addable : std::false_type {};

template<typename T>
struct is_addable<T, std::void_t<decltype(declval<T>() + declval<T>())>> : std::true_type {};

// template + virtual
template<typename T, typename std::enable_if<is_addable<T>::value>* = nullptr>
class FunctorBase {
public:
    virtual void operator()(const T& a, const T& b) = 0;
    virtual ~FunctorBase() = default;
};

// each deri class has its own base class (template specified)
template<typename T>
class FunctorAdd : public FunctorBase<T> {
public:
    void operator()(const T& a, const T& b) override {
        cout << a + b << endl;
    }
    virtual ~FunctorAdd() = default;
};

// trash removed


int main() {

    FunctorAdd<int> a{};
    a(11, 22);

    // FunctorAdd<Test> b{};
    // b(Test{}, Test{});


}