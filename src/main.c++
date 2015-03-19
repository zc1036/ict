
#include <vector>
#include <cstring>
#include <cassert>
#include <iostream>
#include <functional>

#include "ecl/ecl.h"

namespace ictest {
    struct type_mismatch : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

#define TYPE_ASSERT(EXPR, ERRMSG)               \
    if (!(EXPR))                                \
        throw type_mismatch(ERRMSG)

    struct gen_userdata {
        gen_userdata(const std::size_t hash, const char* const name)
            : type_hash(hash), type_name(name) { }

        template<typename T>
        T* try_cast();

        static const unsigned char magic_value = 0b10101010;
        const decltype(magic_value) magic_header = magic_value;
        
        const std::size_t type_hash;
        const char* const type_name;
    };

    template<typename T>
    struct userdata : gen_userdata {
        userdata(T&& t) : gen_userdata(typeid(T).hash_code(), typeid(T).name()),
                          data(std::forward<T>(t)) { }

        T data;
    };

    template<typename T>
    T* gen_userdata::try_cast()  {
        if (type_hash == typeid(T).hash_code() && strcmp(typeid(T).name(), type_name) == 0)
            return &static_cast<userdata<T>*>(this)->data;

        return nullptr;
    }

    template<typename T>
    cl_object convert_c_to_cl(T&& t) {
        return ecl_make_foreign_data(c_string_to_object(typeid(t).name()),
                                     sizeof(userdata<T>),
                                     new userdata<T>(std::forward<T>(t)));
    }

    cl_object convert_c_to_cl(int n) {
        return MAKE_FIXNUM(n);
    }

    std::vector<std::function<cl_object(int, cl_va_list)>> funcs;

    template<typename T>
    T convert_cl_to_c(cl_object obj) {
        TYPE_ASSERT(ECL_FOREIGN_DATA_P(obj), "Expected foreign data");
        TYPE_ASSERT(obj->foreign.size == sizeof(userdata<T>), "Expected userdata<T>");

        auto gen_ptr = reinterpret_cast<gen_userdata*>(ecl_to_pointer(obj));

        TYPE_ASSERT(gen_ptr->magic_header == gen_userdata::magic_value, "Invalid magic header in userdata");

        auto ptr = gen_ptr->try_cast<T>();

        TYPE_ASSERT(ptr != nullptr, "Invalid userdata type");

        return *ptr;
    }

    template<>
    int convert_cl_to_c<int>(cl_object obj) {
        TYPE_ASSERT(FIXNUMP(obj), "Expected integer");

        return fix(obj);
    }

    /*
    template<typename FnRet, typename... FnArgs>
    struct call_fn_from_args {
        using stdfun = std::function<FnRet(FnArgs...)>;

        template<typename T, typename... Args>
        static typename std::enable_if<sizeof...(Args) == 0, cl_object>::type call(stdfun& fn, T&& arg, Args&&... rest) {
            return call(fn, cl_to_c_converter<T>::convert(std::forward<T>(arg)), std::forward<Args>(rest)...);
        }

        template<typename T, typename... Args>
        static typename std::enable_if<sizeof...(Args) != 0, cl_object>::type call(stdfun& fn, T&& arg, Args&&... args) {
            return convert_c_to_cl(fn(cl_to_c_converter<T>::convert(std::forward<T>(arg)),
                                      std::forward<FnArgs>(args)...));
        }
    };
    */

    template<typename Fn, typename... Args>
    struct call_from_va_list;

    template<typename Fn, typename T, typename... ToBeConverted>
    struct call_from_va_list<Fn, T, ToBeConverted...> {
        template<typename... AlreadyConverted>
        static cl_object call(Fn&& fn, cl_narg numargs, cl_va_list list, AlreadyConverted&&... already_converted) {
            if (numargs < 1)
                throw type_mismatch("Not enough arguments to function");

            return call_from_va_list<Fn, ToBeConverted...>::call(std::forward<Fn>(fn),
                                                                 numargs - 1,
                                                                 list,
                                                                 std::forward<AlreadyConverted>(already_converted)...,
                                                                 convert_cl_to_c<T>(cl_va_arg(list)));
        }
    };

    template<typename Fn>
    struct call_from_va_list<Fn> {
        template<typename... AlreadyConverted>
        static cl_object call(Fn&& fn, cl_narg numargs, cl_va_list, AlreadyConverted&&... already_converted) {
            if (numargs != 0)
                throw type_mismatch("Too many arguments to function");

            return convert_c_to_cl(fn(already_converted...));
        }
    };

    struct func_info {
        //func_info(...) { }

        template<typename Ret, typename... Args>
        func_info(std::function<Ret(Args...)> func, std::string n) : name(std::move(n)) {
            thunk = [func = std::move(func)](cl_narg numargs, cl_va_list args) -> cl_object {
                return call_from_va_list<const std::function<Ret(Args...)>&, Args...>::call(func,
                                                                                            numargs,
                                                                                            args);
                
            };
        }

        std::function<cl_object(int, cl_va_list)> thunk;

        std::string name;
    };

    template<unsigned int FuncIndex>
    cl_object cl_trampoline(cl_narg numargs, ...) {
        cl_va_list args;
        cl_va_start(args, numargs, numargs, 0);

        return funcs.at(FuncIndex)(numargs, args);
    }

    template<unsigned int I, unsigned int N>
    struct register_funcs_ {
        static void go(const func_info(&info)[N]) {
            funcs.at(I) = info[I].thunk;

            cl_def_c_function_va(c_string_to_object(info[I].name.c_str()), cl_trampoline<I>);

            register_funcs_<I + 1, N>::go(info);
        }
    };

    template<unsigned int N>
    struct register_funcs_<N, N> {
        static void go(const func_info(&)[N]) { }
    };

    template<unsigned int N>
    void register_funcs(const func_info(&info)[N]) {
        funcs.resize(N);

        register_funcs_<0, N>::go(info);
    }

    template<typename Ret, typename... Args>
    Ret make_struct(Args&&... args) {
        return { std::forward<Args>(args)... };
    }

    /*

      struct S {
          S(int);
      };

      int foo(S);

      register_info<S>::go({
          { make_struct<S, int>, "MAKE-S" },
          { foo, "FOO" }
      });

     */
}

namespace {
    cl_object ecl_call(const char* const call) {
        return cl_safe_eval(c_string_to_object(call), Cnil, Cnil);
    }

    cl_object f(long nargs, ...) {
        std::cout << nargs << std::endl;

        std::cout << "has value? " << std::endl;
        cl_print(1, cl_safe_eval(ecl_call("(read)"), si_environ(), Cnil));
        std::cout << "\ndone" << std::endl;

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

template<typename T>
ictest::func_info stdf(T&& t, std::string c) {
    return { std::forward<T>(t), std::move(c) };
}

template<typename Ret, typename... Args>
std::function<Ret(Args...)> fun(Ret(*fn)(Args...)) {
    return { fn };
}

#include <dlfcn.h>

struct dl_library {
    using handle_t = void*;
    using const_handle_t = const void*;
    
    struct dl_open_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct dl_missing_symbol : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    dl_library(const std::string& libname) {
        if (!(libhandle = dlopen(libname.c_str(), RTLD_LAZY)))
            throw dl_open_error(dlerror());
    }

    dl_library(dl_library&& dying) : libhandle(dying.libhandle) {
        dying.libhandle = nullptr;
    }

    dl_library(const dl_library&) = delete;

    template<typename T>
    T* get_symbol(const std::string& str) {
        assert(handle() != nullptr);

        dlerror(); // clears any previous errors

        auto ret = reinterpret_cast<T*>(dlsym(libhandle, str.c_str()));

        if (auto err = dlerror())
            throw dl_missing_symbol(err);
        
        return ret;
    }

    handle_t handle() {
        return libhandle;
    }

    const_handle_t handle() const {
        return libhandle;
    }

    ~dl_library() {
        if (libhandle)
            dlclose(libhandle);
    }

private:
    handle_t libhandle;
};

struct S {
    int x, y, z;
};

S foo(int x, int y, int z) {
    return { x, y, z };
}

int foo_x(S s) {
    return s.x;
}

int foo_y(S s) {
    return s.y;
}

int foo_z(S s) {
    return s.z;
}

extern "C" int main(/*const int argc, const char* const argv[]*/) {
    /*
    dl_library lib(argv[1]);

    lib.get_symbol<int()>("main");

    return 0;
    */

    {
    char* argv[] { };

    cl_boot(0, argv);
    atexit(cl_shutdown);
    }

    ictest::register_funcs({
            { fun(foo), "FOO" },
            { fun(foo_x), "FOO-X" },
            { fun(foo_y), "FOO-Y" },
            { fun(foo_z), "FOO-Z" }
        });

    auto closure = ecl_make_cclosure_va(f, c_string_to_object("((HI . 534))"), Cnil);
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
