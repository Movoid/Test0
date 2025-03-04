#include "RPNExprParser.h"
#include "Wcnm/wc.h"
#include "mm_malloc_v1.h"
#include <cmath>
#include <iostream>
#include <vector>

int main() {

  wc();
  std::cout << "main()" << std::endl;

  auto sum{[](const std::vector<double> &args) -> double {
<<<<<<< HEAD
    double res{};
    std::cout << "main" << std::endl;
    for (auto a : args)
      res += a;
    return res;
=======
    double sum{};
    std::cout << "test" << std::endl;
    for (auto a : args)
      sum += a;
    return sum;
>>>>>>> test
  }};

  auto max{[](const std::vector<double> &args) -> double {
    double ret{args[0]};
    for (auto a : args)
      ret = std::max(ret, a);
    return ret;
  }};

  auto min{[](const std::vector<double> &args) -> double {
    double ret{args[0]};
    for (auto a : args)
      ret = std::min(ret, a);
    return ret;
  }};

  auto hello{[](const std::vector<double> &args) -> double {
    std::cout << "\nHello, RPNExprParser!" << std::endl;
    return 0;
  }};

  auto exit{[](const std::vector<double> &args) -> double {
    std::exit(0); // NOLINT
    return 0;
  }};

  auto addop{[](double a, double b) -> double { return a + b; }};

  auto subop{[](double a, double b) -> double { return a - b; }};
  auto mulop{[](double a, double b) -> double { return a * b; }};
  auto divop{[](double a, double b) -> double { return a / b; }};
  auto powop{[](double a, double b) -> double { return std::pow(a, b); }};

  ExprParser::RPNExprParser<double> parser{};

  parser.addOp('+', addop, 1);
  parser.addOp('-', subop, 1);
  parser.addOp('*', mulop, 5);
  parser.addOp('/', divop, 5);
  parser.addOp('^', powop, 10);

  parser.addFunc("sum", sum);
  parser.addFunc("max", max);
  parser.addFunc("min", min);
  parser.addFunc("hello_world", hello);
  parser.addFunc("exit", exit);

  parser.addVar("pi", 3.1415926535);
  parser.addVar("e", 2.718);

  std::string in{};
  while (true) {
    std::cout << "Expr: ";
    std::getline(std::cin, in);
    parser.setExpr(in);
    try {
      std::cout << "RPNExpr: " << parser.parseExpr() << std::endl;
      std::cout << "CalcRes: " << parser.calcExpr() << std::endl;
    } catch (const std::exception &e) { std::cout << e.what() << '\n'; }
    std::cout << '\n';
  }

  return 0;
}