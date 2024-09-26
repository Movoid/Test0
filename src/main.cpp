#include <bits/stdc++.h>
#include "RPNExprParser.h"

using namespace std;


int main() {

    auto add{ [](const vector<double>& args)->double {return args[0] + args[1]; } };
    auto sub{ [](const vector<double>& args)->double {return args[0] - args[1]; } };
    auto mul{ [](const vector<double>& args)->double {return args[0] * args[1]; } };
    auto div{ [](const vector<double>& args)->double {return args[0] / args[1]; } };

    auto addop{ [](double a, double b)->double {return a + b; } };
    auto subop{ [](double a, double b)->double {return a - b; } };
    auto mulop{ [](double a, double b)->double {return a * b; } };
    auto divop{ [](double a, double b)->double {return a / b; } };

    ExprParser::RPNExprParser<double> a{};
    a.addFunc("add", add);
    a.addFunc("sub", sub);
    a.addFunc("mul", mul);
    a.addFunc("div", div);

    a.addOp('+', addop, 1);
    a.addOp('-', subop, 1);
    a.addOp('*', mulop, 5);
    a.addOp('/', divop, 5);

    a.addVar("aaa", 200);
    a.addVar("pi", 3.1415926535);

    double pi{ 3.1415926535 };

    std::string input{ "aaa + add(add(1,2), 3)" };
    a.setExpr(input);
    try {
        cout << a.parseExpr() << endl;
        cout << setprecision(10) << a.calcExpr() << endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }






}