#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "strategy.hpp"
#include "histogram.hpp"
#include "reclaimable.hpp"

namespace csot {

struct TickRecord {
    uint64_t timestamp_ns;
    double   bid_px;
    double   ask_px;
    uint32_t bid_qty;
    uint32_t ask_qty;
    uint8_t  sym_idx;
    uint8_t  sym_len;
    uint8_t  _pad[2];
};

class Engine {
public:
    static constexpr std::size_t MAX_SYMBOLS = 64;

    Engine(const char* so_path, const char* csv_path);
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    void run();

    std::vector<Order> run_tick(std::size_t idx);

    std::size_t             tick_count()  const noexcept { return ticks_.size(); }
    const LatencyHistogram& histogram()   const noexcept { return hist_; }

private:
    void    load_strategy(const char* so_path);
    void    load_csv(const char* csv_path);
    uint8_t intern_symbol(const char* p, std::size_t len);

    void*         dl_handle_      = nullptr;
    Strategy*     strategy_       = nullptr;
    IReclaimable* reclaim_iface_  = nullptr;

    std::vector<TickRecord> ticks_;

    char        sym_buf_[MAX_SYMBOLS][16];
    uint8_t     sym_len_[MAX_SYMBOLS];
    std::size_t sym_count_ = 0;

    LatencyHistogram hist_;
};

} // namespace csot
