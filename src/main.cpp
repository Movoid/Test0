#include <bits/stdc++.h>
#include "RPNExprParser.h"

using namespace std;


int main() {

    ExprParser::RPNExprParser<double> a{};
    auto lambda{ [](double a, double b)->double {return a + b; } };
    a.addOp('+', lambda, 1);

}