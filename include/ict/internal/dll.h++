
#ifndef ICTEST_DLL_HPP
#define ICTEST_DLL_HPP

#include <string>
#include <stdexcept>

#include <dlfcn.h>

struct dl_library {
    using handle_t = void*;
    using const_handle_t = const void*;
    
    struct dl_open_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct dl_missing_symbol : std::runtime_error {
        dl_missing_symbol(const std::string& name) : std::runtime_error("Missing symbol"),
                                                     symbol_name(name) { }

        const std::string symbol_name;
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
    T* try_get_symbol(const std::string& name) {
        assert(handle() != nullptr);

        dlerror(); // clears any previous errors

        return reinterpret_cast<T*>(dlsym(libhandle, name.c_str()));
    }

    template<typename T>
    T* get_symbol(const std::string& name) {
        if (auto ret = try_get_symbol<T>(name))
            return ret;

        throw dl_missing_symbol(name);
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

#endif
