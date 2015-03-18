
#include <vector>
#include <cassert>
#include <iostream>
#include <functional>

#include "ecl/ecl.h"

namespace ecl {
    template<typename T>
    struct cl_to_c_converter {
        static T convert(cl_object);
    };

    template<>
    struct cl_to_c_converter<int> {
        static int convert(cl_object obj) {
            assert(FIXNUMP(obj));

            return fix(obj);
        }
    };

    template<typename T>
    struct c_to_cl_converter {
        static cl_object convert(T&&);
    };

    std::vector<std::function<cl_object(int, cl_va_list)>> funcs;

    template<unsigned int FuncIndex>
    cl_object cl_trampoline(int numargs, ...) {
        cl_va_list args;
        cl_va_start(args, numargs, numargs, 0);

        funcs.at(FuncIndex)(numargs, args);

        return Cnil;
    }

    template<typename FnRet, typename... FnArgs>
    struct call_fn_from_args {
        using stdfun = std::function<FnRet(FnArgs...)>;

        template<typename T, typename... Args>
        static typename std::enable_if<sizeof...(Args) == 0, cl_object>::type call(stdfun& fn, T&& arg, Args&&... rest) {
            return call(fn, cl_to_c_converter<T>::convert(std::forward<T>(arg)), std::forward<Args>(rest)...);
        }

        template<typename T, typename... Args>
        static typename std::enable_if<sizeof...(Args) != 0, cl_object>::type call(stdfun& fn, T&& arg, Args&&... args) {
            return c_to_cl_converter<FnRet>::convert(fn(cl_to_c_converter<T>::convert(std::forward<T>(arg)),
                                                        std::forward<FnArgs>(args)...));
        }
    };

    template<unsigned int FuncIndex, typename Ret, typename... Args>
    void register_function(const std::string& name, std::function<Ret(Args...)>& fn) {
        funcs.at(FuncIndex) = [fn](int numargs, cl_va_list args) {
            
        };

        //cl_def_c_function(c_string_to_object(name.data()), );
    }
}

namespace {
    cl_object ecl_call(const char* const call) {
        return cl_safe_eval(c_string_to_object(call), Cnil, Cnil);
    }

    cl_object f(long nargs, ...) {
        std::cout << nargs << std::endl;

        std::cout << "has value? " << std::endl;
        
        cl_print(1, cl_boundp(c_string_to_object("HI")));
        std::cout << "done2" << std::endl;
        cl_print(1, cl_symbol_value(c_string_to_object("HI")));
        std::cout << "done" << std::endl;

        return Cnil;
    }

    bool repl_continue = true;

    cl_object exit_func() {
        repl_continue = false;

        return Cnil;
    }
}

// cl_object cl_s2f(cl_object lol) {
//     assert(STRINGP(lol));

//     std::string foo(lol->string.self, lol->string.fillp);

    
// }

int main() {
    char* argv[] { };

    cl_boot(0, argv);
    atexit(cl_shutdown);

    auto closure = ecl_make_cclosure_va(f, c_string_to_object("((HI . 2))"), c_string_to_object("((HI . 2))"));
    ecl_defparameter(c_string_to_object("HILOL"), closure);

    cl_def_c_function(c_string_to_object("EXIT"), exit_func, 0);
    //cl_def_c_function(c_string_to_object("CL-S2F"), c_string_to_object, 1);

    std::cout << "ICTEST\n"
                 "call EXIT to quit\n\n";

    std::size_t inputno = 1;

    do {
        std::cout << inputno++ << " > " << std::flush;

        cl_print(1, cl_safe_eval(ecl_call("(read)"), Cnil, Cnil));

        std::cout << "\n";
    } while (repl_continue);
}
