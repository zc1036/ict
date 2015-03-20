
#include <vector>
#include <cstring>
#include <cassert>
#include <iostream>

#include <ict/internal/dll.h++>
#include <ict/ict.h++>

#include "ecl/ecl.h"

extern "C" {
    using init_fn = void();
    using register_fn = void();
}

void usage(const int argc, const char* const argv[]) {
    std::cout << "Usage: " << *argv << " BINARY" << std::endl;
}

int main(const int argc, const char* const argv[]) {
    if (argc < 2) {
        usage(argc, argv);
        return 1;
    }

    init_fn* lib_init = nullptr;
    register_fn* register_funcs;
    
    try {
        dl_library lib(argv[1]);

        lib_init = lib.try_get_symbol<init_fn>("ict_init");
        register_funcs = lib.get_symbol<init_fn>("ict_register");

        {
            char* fake_argv[1] { };
    
            cl_boot(0, fake_argv);
            atexit(cl_shutdown);
        }

        if (lib_init)
            lib_init();

        register_funcs();

        cl_eval(c_string_to_object("(si:top-level t)"));
    } catch (const dl_library::dl_open_error& err) {
        std::cerr << err.what() << '\n'
                  << "If the binary file exists and is an executable but can't be loaded, and you\n"
                     "used GCC to compile it, make sure the following flags were used:\n"
                     "-fPIC -pie -rdynamic" << std::endl;
        return 1;
    } catch (const dl_library::dl_missing_symbol& err) {
        std::cerr << "Binary " << argv[1] << " missing symbol " << err.symbol_name << std::endl
                  << "A binary needs to define and export the symbol ict_register and optionally\n"
                      "the symbol ict_init" << std::endl;
        return 1;
    }
}
