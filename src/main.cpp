#include <bits/stdc++.h>
#include "RPNExprParser.h"

using namespace std;

class A {
public:
    int a{};
    A(int _a) :
        a{ _a } {
        if (_a == 1) throw std::runtime_error{ "woc" };
    }

    A(A&& obj) :
        a{ obj.a } {
        cout << "move" << endl;
    }

};

unordered_map<int, A> mp{};


int main() {


    try {
        mp.emplace(1, std::move(A{ 1 }));
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }



}