#include <vector>
#include <cmath>
#include <string>
#include <unordered_map>
#include <exception>
#include <type_traits>

namespace ExprParser {

    template<typename T, typename Requires = typename std::enable_if_t<!std::is_same_v<T, void>>>
    class CustomFunctionWrapper {
    private:
        std::function<T(const std::vector<T>&)> func{};
        std::size_t argCnt{};
        bool limitedArgc{};
        bool invalid{};
    public:
        CustomFunctionWrapper() :
            invalid{ true } {
        }
        CustomFunctionWrapper(const std::function<T(const std::vector<T>&)>& _func, size_t _argCnt = 0, bool _limitedArgc = false) :
            func{ _func },
            argCnt{ _argCnt },
            limitedArgc{ _limitedArgc } {
            if (argCnt && !limitedArgc) throw std::runtime_error{ "[FunctionCall] 函数参数限制初始化错误." };
            // std::cout << "ctor" << std::endl;
        }
        CustomFunctionWrapper(const CustomFunctionWrapper& obj) = default;
        CustomFunctionWrapper(const CustomFunctionWrapper&& obj) :
            func{ std::move(obj.func) },
            argCnt{ obj.argCnt },
            limitedArgc{ obj.limitedArgc } {
            // std::cout << "move" << std::endl;
        }
        ~CustomFunctionWrapper() = default;

        T operator()(const std::vector<T>& args) const {
            if (limitedArgc && args.size() != argCnt) throw std::runtime_error{ "[FunctionCall] 无效的函数调用, 参数不匹配." };
            return func(args);
        }

        inline bool isInvalid() { return invalid; }
    };

    template<typename T, typename Requires = typename std::enable_if_t<!std::is_same_v<T, void>>>
    class CustomOperatorWrapper {
    private:
        std::function<T(const T&, const T&)> func{};
        int priority{};
        bool invalid{};
    public:
        CustomOperatorWrapper() :
            invalid{ true } {
        }
        CustomOperatorWrapper(const std::function<T(const T&, const T&)>& _func, int _priority) :
            func{ _func },
            priority{ _priority } {
        }
        CustomOperatorWrapper(const CustomOperatorWrapper& obj) = default;
        CustomOperatorWrapper(const CustomOperatorWrapper&& obj) :
            func{ std::move(obj.func) },
            priority{ obj.priority } {
        }
        ~CustomOperatorWrapper() = default;

        T operator()(const T& a, const T& b) const {
            return func(a, b);
        }

        friend bool operator<(const CustomOperatorWrapper& l, const CustomOperatorWrapper& r) {
            return l.priority < r.priority;
        }

        inline bool isInvalid() { return invalid; }
    };

    template<typename T>
    using Functor = CustomFunctionWrapper<T>;

    template<typename T, typename Requires = typename std::enable_if_t<!std::is_same_v<T, void>>>
    using Operator = CustomOperatorWrapper<T>;

    template<typename T>
    class RPNExprParser {
    private:
        using OpTable = std::unordered_map<char, Operator<T>>;
        using VarTable = std::unordered_map<std::string_view, T>;
        using FuncTable = std::unordered_map<std::string_view, Functor<T>>;

        std::string originExpr{};
        char delim{ ' ' };
        OpTable ops{};
        VarTable vars{};
        FuncTable funcs{};
        std::string rpnExpr{};
        T calcRes;
        bool isCalced{};

    protected:

        inline bool isAlpha(char c) {
            return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_');
        }
        inline bool isDigit(char c) {
            return (('0' <= c && c <= '9') || c == '.');
        }
        inline bool isDelim(char c) {
            return c == delim;
        }
        inline bool isOp(char c) {
            return (!isAlpha(c) && !isDigit(c) && !isDelim(c) && c != '(' && c != ')');
        }

        // 只进行字符串处理
        std::string parse(std::size_t beg, std::size_t end) {
            std::stack<char> opStack{};
            std::string rpn{};

            std::size_t l{ beg }, r{ beg };
            while (l < end) {
                while (l < end && isDelim(originExpr[l])) ++l;
                if (l >= end) break;
                r = l;
                // main parse
                if (isDigit(originExpr[l])) {
                    // 一定是数字
                    while (r < end && isDigit(originExpr[r])) ++r;
                    rpn += (originExpr.substr(l, r - l));
                    rpn += ' ';
                    l = r;
                    continue;
                }
                // 解析括号
                if (originExpr[l] == '(') {
                    ++r;
                    opStack.emplace('(');
                    l = r;
                    continue;
                }
                if (originExpr[l] == ')') {
                    ++r;
                    while (!opStack.empty() && opStack.top() != '(') {
                        if (opStack.top() == '(') break;
                        rpn += opStack.top();
                        rpn += ' ';
                        opStack.pop();
                    }
                    if (opStack.empty()) throw std::runtime_error{ "[Parser] 括号不匹配." };
                    opStack.pop();
                    l = r;
                    continue;
                }
                if (isOp(originExpr[l])) {
                    // 一定是单个 char 的运算符
                    ++r;
                    if (ops.count(originExpr[l])) {
                        while (!opStack.empty() && !(ops[opStack.top()] < ops[originExpr[l]])) {
                            rpn += opStack.top();
                            rpn += ' ';
                            opStack.pop();
                        }
                        opStack.emplace(originExpr[l]);
                    }
                    else {
                        throw std::runtime_error{ "[Parser] 表达式存在无效的操作符." };
                    }
                    l = r;
                    continue;
                }
                if (isAlpha(originExpr[l])) {
                    while (r < end && (isDigit(originExpr[r]) || isAlpha(originExpr[r]))) {
                        ++r;
                    }
                    if (r < end && originExpr[r] == '(') {
                        // is function
                        throw std::runtime_error{ "NOT IMPLEMENTED." };
                    }
                    else {
                        // is variant
                        std::string_view curVarName{ originExpr.substr(l, r - l) };
                        if (vars.count(curVarName)) {
                            rpn += curVarName;
                            rpn += ' ';
                        }
                        else {
                            throw std::runtime_error{ "[Parser] 表达式存在无效的变量." };
                        }
                    }
                    l = r;
                    continue;
                }

                throw std::runtime_error{ "[Parser] 不合法的字符." };
            }
            while (!opStack.empty()) {
                if (opStack.top() == '(') throw std::runtime_error{ "[Parser] 括号不匹配." };
                rpn += opStack.top();
                rpn += ' ';
                opStack.pop();
            }
            return rpn;
        }

        T calc() {
            std::stack<T> numStack{};
            std::size_t l{}, r{};
            while (r < rpnExpr.size()) {



            }
        }

    public:
        RPNExprParser() {};
        RPNExprParser(
            std::string_view _originExpr,
            char _delim = ' ',
            const OpTable& _ops = {},
            const VarTable& _vars = {},
            const FuncTable& _funcs = {}) :
            originExpr{ _originExpr },
            delim{ _delim },
            ops{ _ops },
            vars{ _vars },
            funcs{ _funcs } {
        }
        RPNExprParser(const RPNExprParser& obj) = delete;
        RPNExprParser(const RPNExprParser&& obj) = delete;
        ~RPNExprParser() = default;

        void addOp(char name, const std::function<T(const T&, const T&)>& op, int priority) {
            if (ops.count(name)) throw std::runtime_error{ "[Controller] 已存在的操作符." };
            ops.emplace(name, Operator<T>{op, priority});
        }
        void addVar(std::string_view name, const T& var) {
            if (vars.count(name)) throw std::runtime_error{ "[Controller] 已存在的变量名." };
            vars.emplace(name, var);
        }
        void addFunc(std::string_view name, const std::function<T(const std::vector<T>&)>& func, size_t argCnt = 0, bool limitedArgc = false) {
            if (funcs.count(name)) throw std::runtime_error{ "[Controller] 已存在的函数名." };
            funcs.emplace(name, Functor<T>{ func, argCnt, limitedArgc });
        }
        void removeOp(char name) { ops.erase(name); }
        void removeVar(std::string_view name) { vars.erase(name); }
        void removeFunc(std::string_view name) { funcs.erase(name); }
        void clearOp() { ops.clear(); }
        void clearVar() { vars.clear(); }
        void clearFuncs() { funcs.clear(); }
        void setExpr(std::string_view _originExpr, char delim = ' ') {
            originExpr = _originExpr;
            rpnExpr.clear();
            isCalced = false;
        }

        std::string_view parseExpr() {
            if (rpnExpr.empty()) rpnExpr = std::move(parse(0, originExpr.size()));
            return rpnExpr;
        }

        const T& calcExpr() {
            if (rpnExpr.empty()) parseExpr();
            if (!isCalced) {
                calcRes = std::move(calc());
                isCalced = true;
            }
            return calcRes;
        }




    };

};

