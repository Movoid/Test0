#include <bits/stdc++.h>
#include "RPNExprParser.h"

int main() {

    auto sum{ [](const std::vector<double>& args)->double {
        double sum{};
        for (auto a : args) sum += a;
        return sum;
    } };

    auto max{ [](const std::vector<double>& args)->double {
        double ret{args[0]};
        for (auto a : args) ret = std::max(ret, a);
        return ret;
    } };

    auto min{ [](const std::vector<double>& args)->double {
        double ret{args[0]};
        for (auto a : args) ret = std::min(ret, a);
        return ret;
    } };

    auto hello{ [](const std::vector<double>& args)->double {
        std::cout << "\nHello, RPNExprParser!" << std::endl;
        return 0;
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
    a.addFunc("hello_world", hello);

    a.addVar("pi", 3.1415926535);
    a.addVar("e", 2.718);

    std::string in{};
    while (true) {
        std::cout << "Expr: ";
        std::getline(std::cin, in);
        if (in == "exit") break;
        a.setExpr(in);
        try {
            std::cout << "RPNExpr: " << a.parseExpr() << std::endl;
            std::cout << "CalcRes: " << a.calcExpr() << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << e.what() << '\n';
        }
        std::cout << '\n';
    }

    return 0;
}