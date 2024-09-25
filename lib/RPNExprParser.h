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
        bool limitedArgc{};
    public:
        CustomFunctionWrapper(const std::function<T(const std::vector<T>&)> _func, size_t _argCnt = 0, bool _limitedArgc = false) :
            func{ _func },
            argCnt{ _argCnt },
            limitedArgc{ _limitedArgc } {
            if (argCnt && !limitedArgc) throw std::runtime_error{ "[FunctionCall] 函数参数限制初始化错误." };
        }
        CustomFunctionWrapper(const CustomFunctionWrapper& obj) = default;
        CustomFunctionWrapper(const CustomFunctionWrapper&& obj) :
            func{ std::move(obj.func) },
            argCnt{ obj.argCnt },
            limitedArgc{ obj.limitedArgc } {
        }
        ~CustomFunctionWrapper() = default;

        T operator()(const std::vector<T>& args) const {
            if (limitedArgc && args.size() != argCnt) throw std::runtime_error{ "[FunctionCall] 无效的函数调用, 参数不匹配." };
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
        CustomOperatorWrapper(const CustomOperatorWrapper&& obj) :
            func{ std::move(obj.func) },
            priority{ obj.priority } {
        }
        CustomOperatorWrapper() = default;

        T operator()(const T& a, const T& b) const {
            return func(a, b);
        }

        friend bool operator<(const CustomOperatorWrapper& l, const CustomOperatorWrapper& r) {
            return l.priority < r.priority;
        }
    };

    template<typename T>
    using Functor = CustomFunctionWrapper<T>;

    template<typename T, typename Requires = typename std::enable_if_t<!std::is_same_v<T, void>>>
    using Operator = CustomOperatorWrapper<T>;

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
            if (ops.count(name)) throw std::runtime_error{ "[RPNExprParser] 已存在的操作符." };
            ops.emplace(name, std::move(Operator<T>{op, priority}));
        }
        void addVar(std::string_view name, const T& var) {
            if (vars.count(name)) throw std::runtime_error{ "[RPNExprParser] 已存在的变量名." };
            vars.emplace(name, var);
        }
        void addFunc(std::string_view name, const Functor<T>& func, size_t argCnt = 0, bool limitedArgc = false) {
            if (funcs.count(name)) throw std::runtime_error{ "[RPNExprParser] 已存在的函数名." };
            funcs.emplace(name, std::move(Functor<T>{func, argCnt, limitedArgc}));
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

