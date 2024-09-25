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

    string str1{ "nihao" };
    string_view str2{ str1 };
    str1 = "zaijian";
    cout << str2 << endl;

    a.addVar("aaa", 200);
    std::string input{ "   (1 + 3 )* 2 + aaa" };
    a.setExpr(input);
    try {
        cout << a.parse(0, input.size()) << endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }





}