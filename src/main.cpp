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


int main() {

    A obj1{};
    B obj2{};
    obj1.interface(1, 2);
    obj2.interface(3, 4);

}