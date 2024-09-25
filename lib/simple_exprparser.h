#include <bits/stdc++.h>
using ll = long long;
using ull = unsigned long long;

namespace ExprParser {
    inline bool is_delim(char op) {
        return op == ' ';
    }

    inline bool is_op(char c) {
        return c == '+' || c == '-' || c == '*' || c == '/';
    }

    inline int priority(char op) {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;  // higher priority
        return -1;  // '(' and ')'
    }

    std::string parse(std::string_view s) {

        std::stack<char> op{};
        std::string res{};

        int l{};
        while (l < s.size()) {
            if (is_delim(s[l])) {
                ++l;
                continue;
            }
            if (isdigit(s[l])) {
                while (l < s.size() && isdigit(s[l])) {
                    res += s[l++];
                }
                res += ' ';
            }
            else if (s[l] == '(') {
                op.emplace('(');
                ++l;
            }
            else if (s[l] == ')') {
                while (!op.empty() && op.top() != '(') {
                    res += op.top();
                    res += ' ';
                    op.pop();
                }
                op.pop();
                ++l;
            }
            else if (is_op(s[l])) {
                while (!op.empty() && priority(s[l]) <= priority(op.top())) {
                    if (op.top() == '(') break;
                    res += op.top();
                    res += ' ';
                    op.pop();
                }
                op.emplace(s[l]);
                ++l;
            }
            else {
                throw 1;
            }
        }
        while (!op.empty()) {
            res += op.top();
            res += ' ';
            op.pop();
        }

        return res;

    }

    int calc(std::string_view expr) {

        std::stack<int> num{};

        int l{}, r{};
        while (l < expr.size()) {
            while (l < expr.size() && expr[l] == ' ') {
                ++l;
            }
            r = l;
            while (r < expr.size() && expr[r] != ' ') {
                ++r;
            }
            if (l >= expr.size()) break;
            std::string token{ expr.substr(l, r - l) };

            if (isdigit(token[0])) {
                num.emplace(stoi(token));
            }
            else {
                int b{ num.top() };
                num.pop();
                int a{ num.top() };
                num.pop();
                int res{};
                switch (token[0]) {
                    case '+':
                        res = a + b;
                        break;
                    case '-':
                        res = a - b;
                        break;
                    case '*':
                        res = a * b;
                        break;
                    case '/':
                        res = a / b;
                        break;
                    default:
                        throw 2;
                        break;
                }
                num.emplace(res);
            }
            l = r;
        }

        return num.top();
    }
}