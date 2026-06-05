#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>

#include "engine.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::fprintf(stderr,
                     "usage: %s <strategy.so> <ticks.csv>\n", argv[0]);
        return 1;
    }

    const char* so_path  = argv[1];
    const char* csv_path = argv[2];

    try {
        csot::Engine engine(so_path, csv_path);
        engine.run();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "fatal: %s\n", ex.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "fatal: unknown exception\n");
        return 1;
    }

    return 0;
}
