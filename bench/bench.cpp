#include <benchmark/benchmark.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "engine.hpp"

static const char* g_so_path  = "spec_strategy.so";
static const char* g_csv_path = "data/synthetic_small.csv";

static void BM_EngineFullReplay(benchmark::State& state) {
    csot::Engine eng(g_so_path, g_csv_path);
    const long long n = static_cast<long long>(eng.tick_count());

    for (auto _ : state) {
        for (std::size_t i = 0, e = static_cast<std::size_t>(n); i < e; ++i) {
            auto v = eng.run_tick(i);
            benchmark::DoNotOptimize(v.data());
            benchmark::ClobberMemory();
        }
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_EngineFullReplay)->Unit(benchmark::kNanosecond);

static void BM_EngineSingleTick(benchmark::State& state) {
    csot::Engine eng(g_so_path, g_csv_path);
    const std::size_t n = eng.tick_count();
    if (n < 256) {
        state.SkipWithError("CSV too short: need >= 256 ticks for warm-up");
        return;
    }

    for (std::size_t i = 0; i < 256; ++i) {
        auto v = eng.run_tick(i);
        benchmark::DoNotOptimize(v.data());
    }

    std::size_t i = 256;
    for (auto _ : state) {
        auto v = eng.run_tick(i);
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
        if (++i >= n) i = 256;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EngineSingleTick)->Unit(benchmark::kNanosecond);

int main(int argc, char** argv) {
    if (const char* e = std::getenv("CSOT_SO"))  g_so_path  = e;
    if (const char* e = std::getenv("CSOT_CSV")) g_csv_path = e;

    int new_argc = 1;
    std::vector<char*> new_argv;
    new_argv.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--so=",  5) == 0) {
            g_so_path  = argv[i] + 5;
        } else if (std::strncmp(argv[i], "--csv=", 6) == 0) {
            g_csv_path = argv[i] + 6;
        } else {
            new_argv.push_back(argv[i]);
            ++new_argc;
        }
    }

    ::benchmark::Initialize(&new_argc, new_argv.data());
    if (::benchmark::ReportUnrecognizedArguments(new_argc, new_argv.data()))
        return 1;

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
