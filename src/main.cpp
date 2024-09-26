#include <bits/stdc++.h>
#include "RPNExprParser.h"



int main() {

    auto sum{ [](const std::vector<double>& args) {
        double sum{};
        for (auto a : args) sum += a;
        return sum;
    } };

    auto max{ [](const std::vector<double>& args) {
        double ret{args[0]};
        for (auto a : args) ret = std::max(ret, a);
        return ret;
    } };

    auto min{ [](const std::vector<double>& args) {
        double ret{args[0]};
        for (auto a : args) ret = std::min(ret, a);
        return ret;
    } };

    auto addop{ [](double a, double b)->double {return a + b; } };
    auto subop{ [](double a, double b)->double {return a - b; } };
    auto mulop{ [](double a, double b)->double {return a * b; } };
    auto divop{ [](double a, double b)->double {return a / b; } };
    auto powop{ [](double a, double b)->double {return std::pow(a,b); } };

    ExprParser::RPNExprParser<double> a{};

    a.addOp('+', addop, 1);
    a.addOp('-', subop, 1);
    a.addOp('*', mulop, 5);
    a.addOp('/', divop, 5);
    a.addOp('^', powop, 10);

    a.addFunc("sum", sum);
    a.addFunc("max", max);
    a.addFunc("min", min);

    std::string input{ "max(sum(1,2,3,4.4) * 5, 60, 70^2)" };
    a.setExpr(input);
    try {
        std::cout << a.parseExpr() << std::endl;
        std::cout << std::setprecision(10) << a.calcExpr() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }

}