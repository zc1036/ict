
#ifndef ICT_HPP
#define ICT_HPP

#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <functional>
#include <type_traits>

#include <ecl/ecl.h>

#define CL_ASSERT(EXPR, ERRMSG)                                         \
    do {                                                                \
        if (!(EXPR))                                                    \
            cl_error(1, ecl_make_simple_base_string(strdup((ERRMSG)), -1)); \
    } while (0)

namespace ict {
    struct gen_userdata {
        gen_userdata(const std::size_t hash, const char* const name)
            : type_hash(hash), type_name(name) { }

        template<typename T>
        typename std::remove_reference<T>::type* try_cast();

        static const unsigned short magic_value = 0x1033;
        const decltype(magic_value) magic_header = magic_value;
        
        const std::size_t type_hash;
        const char* const type_name;
    };

    template<typename T>
    struct userdata : gen_userdata {
        userdata(T&& t) : gen_userdata(typeid(T).hash_code(), typeid(T).name()),
                          data(std::forward<T>(t)) { }

        typename std::remove_reference<T>::type data;
    };

    template<typename RefT>
    typename std::remove_reference<RefT>::type* gen_userdata::try_cast()  {
        using T = typename std::remove_reference<RefT>::type;
        
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

    inline cl_object convert_c_to_cl(int n) {
        return MAKE_FIXNUM(n);
    }

    std::vector<std::function<cl_object(int, cl_va_list)>> funcs;

    template<typename T>
    T convert_cl_to_c(cl_object obj) {
        CL_ASSERT(ECL_FOREIGN_DATA_P(obj), "Expected foreign data");
        CL_ASSERT(obj->foreign.size == sizeof(userdata<T>), "Expected userdata<T>");

        auto gen_ptr = reinterpret_cast<gen_userdata*>(ecl_to_pointer(obj));

        CL_ASSERT(gen_ptr->magic_header == gen_userdata::magic_value, "Invalid magic header in userdata");

        auto ptr = gen_ptr->try_cast<T>();

        CL_ASSERT(ptr != nullptr, "Invalid userdata type");

        return *ptr;
    }

    template<>
    inline int convert_cl_to_c<int>(cl_object obj) {
        CL_ASSERT(FIXNUMP(obj), "Expected integer");

        return fix(obj);
    }

    template<>
    inline std::string convert_cl_to_c<std::string>(cl_object obj) {
        CL_ASSERT(ECL_BASE_STRING_P(obj), "Expected base string");

        return std::string(ecl_base_string_pointer_safe(obj), obj->base_string.fillp);
    }

    template<typename Fn, typename... Args>
    struct call_from_va_list;

    template<typename Fn, typename T, typename... ToBeConverted>
    struct call_from_va_list<Fn, T, ToBeConverted...> {
        template<typename... AlreadyConverted>
        static cl_object call(Fn&& fn, cl_narg numargs, cl_va_list list, AlreadyConverted&&... already_converted) {
            CL_ASSERT(numargs > 0, "Not enough arguments to function");

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
        static auto call(Fn&& fn, cl_narg numargs, cl_va_list, AlreadyConverted&&... already_converted)
            -> typename std::enable_if<!std::is_void<decltype(fn(already_converted...))>{}, cl_object>::type {
            CL_ASSERT(numargs == 0, "Too many arguments to function");

            return convert_c_to_cl(fn(already_converted...));
        }

        template<typename... AlreadyConverted>
        static auto call(Fn&& fn, cl_narg numargs, cl_va_list, AlreadyConverted&&... already_converted)
            -> typename std::enable_if<std::is_void<decltype(fn(already_converted...))>{}, cl_object>::type {
            CL_ASSERT(numargs == 0, "Too many arguments to function");

            fn(already_converted...);

            return Cnil;
        }
    };

    namespace internal {
        template<typename T, typename Ret, typename... Args>
        Ret ret_type(Ret(T::*)(Args...));

        template<typename... Args>
        struct std_function_type;

        template<typename T, typename Ret, typename... Args>
        struct std_function_type<Ret(T::*)(Args...)> {
            using type = std::function<Ret(Args...)>;
        };

        template<typename T, typename Ret, typename... Args>
        struct std_function_type<Ret(T::*)(Args...) const> {
            using type = std::function<Ret(Args...)>;
        };

        template<typename Ret, typename... Args>
        std::function<Ret(Args...)> stdfun(Ret(*fn)(Args...)) {
            return { fn };
        }

        template<typename T>
        auto stdfun(T&& fn) {
            using realtype = typename std::remove_reference<T>::type;
            return typename std_function_type<decltype(&realtype::operator())>::type { fn };
        }

        template<typename Ret, typename... Args>
        auto functor_call_stdfunc_from_va_list(std::function<Ret(Args...)>&& func) {
            return [func = std::move(func)](cl_narg numargs, cl_va_list args) {
                return call_from_va_list<const std::function<Ret(Args...)>&, Args...>::call(func,
                                                                                            numargs,
                                                                                            args);
            };
        }
    }

    struct func_info {
        template<typename F>
        func_info(F&& func, std::string n) : name(std::move(n)),
                                             thunk(internal::functor_call_stdfunc_from_va_list(internal::stdfun(func)))
        { }

        std::string name;
        std::function<cl_object(int, cl_va_list)> thunk;
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
        return Ret { std::forward<Args>(args)... };
    }
}

#undef CL_ASSERT

#endif
