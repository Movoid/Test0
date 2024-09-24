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
        size_t argCnt{};
    public:
        CustomFunctionWrapper(const std::function<T(const std::vector<T>&)> _func, size_t _argCnt) :
            func{ _func },
            argCnt{ _argCnt } {
        }
        CustomFunctionWrapper(const CustomFunctionWrapper& obj) = default;
        ~CustomFunctionWrapper() = default;

        T operator()(const std::vector<T>& args) const {
            if (args.size() != argCnt) throw std::runtime_error{ "[FunctionCall] 无效的函数调用, 参数不匹配." };
            return func(args);
        }
    };

    template<typename T, typename Requires = typename std::enable_if_t<!std::is_same_v<T, void>>>
    class CustomOperatorWrapper {
    private:
        std::function<T(const T&, const T&)> func{};
        int priority{};
    public:
        CustomOperatorWrapper(const std::function<T(const T&, const T&)>& _func, int _priority) :
            func{ _func },
            priority{ _priority } {
        }
        CustomOperatorWrapper(const CustomOperatorWrapper& obj) = default;
        CustomOperatorWrapper() = default;

        T operator()(const T& a, const T& b) const {
            return func(a, b);
        }
    };

    template<typename T>
    using Functor = CustomFunctionWrapper<T>;

    template<typename T>
    using Operator = CustomOperatorWrapper<T>;

    // 自带
    Operator<double> add{ [](double a, double b)->double {return a + b; }, 1 };
    Operator<double> sub{ [](double a, double b)->double {return a - b; }, 1 };
    Operator<double> mul{ [](double a, double b)->double {return a * b; }, 5 };
    Operator<double> div{ [](double a, double b)->double {return a / b; }, 5 };

    template<typename T, typename Requires = typename std::void_t<decltype(T{}) > >
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
        T calcRes{};

    public:
        RPNExprParser() {};
        RPNExprParser(
            std::string_view _originExpr,
            char _delim = ' ',
            const OpTable& _ops = { {'+', add}, {'-', sub}, {'*', mul}, {'/', div} },
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

        void addOp(char name, const Operator<T>& op) {
            if (ops.count(name)) throw std::runtime_error{ "[RPNExprParser] 已存在的操作符." };
            ops.emplace(name, op);
        }
        void addVar(std::string_view name, const T& var) {
            if (vars.count(name)) throw std::runtime_error{ "[RPNExprParser] 已存在的变量名." };
            vars.emplace(name, var);
        }
        void addFunc(std::string_view name, const Functor<T>& func) {
            if (funcs.count(func)) throw std::runtime_error{ "[RPNExprParser] 已存在的函数名." };
            funcs.emplace(name, func);
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
            calcRes = {};
        }


    };

};

