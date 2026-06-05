# CSoT 2026 - Low Latency Week 1

Synchronous C++ trading platform runner. Replays historical ticks against a z-score mean-reversion strategy, measures per-decision latency, and verifies correctness against the judge reference.

## Files

**include/**
- `strategy.hpp` -- frozen ABI: Tick, Order, Strategy base class, create_strategy() factory. Do not modify.
- `histogram.hpp` -- exponential-bucket latency histogram used by the engine. Do not modify.
- `engine.hpp` -- Engine class declaration: CSV loader, dlopen wrapper, replay loop, LatencyHistogram.

**src/**
- `engine.cpp` -- Engine implementation. Parses CSV with a manual ASCII parser, interns symbols in a flat 64-entry table, runs the tick loop with steady_clock timing, applies the deterministic fill model.
- `main.cpp` -- Entry point. Accepts `<strategy.so> <ticks.csv>`, constructs the engine, runs the replay, prints the latency summary.

**strategies/**
- `spec_strategy.cpp` -- Concrete implementation of STRATEGY_SPEC.md. Per-symbol ring buffer of 64 mid-prices, cache-line-aligned state, population z-score computed from scratch each tick to match the judge reference exactly. Exports `create_strategy()`.

**bench/**
- `bench.cpp` -- Google Benchmark driver. Two benchmarks: full replay throughput and single-tick latency on a warm strategy. Accepts `--so=` and `--csv=` flags. Use `--benchmark_out=results.json --benchmark_out_format=json` for JSON output.

**data/**
- `gen.py` -- Tick generator. Seed 42 produces the cohort baseline. Usage: `python3 data/gen.py --rows 10000 --out data/synthetic_small.csv`
- `tiny.csv` -- 20-row golden file for quick sanity checks.

**.github/workflows/build.yml** -- CI workflow that builds `spec_strategy.so` on Ubuntu x86-64 and saves it as a downloadable artifact for portal submission.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=OFF
cmake --build build -j
```

## Run

```bash
python3 data/gen.py --rows 10000 --out data/synthetic_small.csv
./build/quant_runner ./build/spec_strategy.dylib data/synthetic_small.csv
```

## Benchmark

```bash
./build/quant_bench --so=./build/spec_strategy.dylib \
                    --csv=data/synthetic_small.csv \
                    --benchmark_out=results.json \
                    --benchmark_out_format=json
```
## Strategy

Rolling 64-tick z-score mean reversion per symbol. Entry at `|z| >= 2.0`, exit at `|z| <= 0.5`, max position 1 unit per symbol. Full algorithm in `STRATEGY_SPEC.md`.
