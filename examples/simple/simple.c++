
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <unordered_map>

#include <ict/ict.h++>

using std::begin;
using std::end;

using namespace std::literals::chrono_literals;

using map_t = std::unordered_map<std::string, int>; 

int umap_get(map_t& map, std::string key) {
    return map.at(key);
}

void umap_set(map_t& map, std::string key, int value) {
    map[key] = value;
}

extern "C" void ict_register() {
    int my_special_int = 42;

    ict::register_funcs({
        { umap_get,                                      "UMAP-GET" },
        { umap_set,                                      "UMAP-SET" },
        { ict::make_struct<map_t>,                       "MAKE-UMAP" },
        { [my_special_int]() { return my_special_int; }, "MY-SPECIAL-INT" }
    });
}

int main() {
    std::cout << "Hey Zorak" << std::flush;

    for (int i = 0; i < 3; ++i) {
        std::cout << '.' << std::flush;
        std::this_thread::sleep_for(.75s);
    }

    std::cout << " You're bringing me DOWN man!" << std::endl;
}
