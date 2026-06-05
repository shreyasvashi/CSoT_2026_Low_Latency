#include "engine.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace csot {

namespace {

using CreateFn = Strategy*(*)();

[[nodiscard]] static uint64_t parse_u64(const char*& p) noexcept {
    uint64_t v = 0;
    while (static_cast<unsigned>(*p - '0') < 10u)
        v = v * 10u + static_cast<uint64_t>(*p++ - '0');
    return v;
}

[[nodiscard]] static uint32_t parse_u32(const char*& p) noexcept {
    uint32_t v = 0;
    while (static_cast<unsigned>(*p - '0') < 10u)
        v = v * 10u + static_cast<uint32_t>(*p++ - '0');
    return v;
}

[[nodiscard]] static double parse_price(const char*& p) noexcept {
    uint64_t intpart = 0;
    while (static_cast<unsigned>(*p - '0') < 10u)
        intpart = intpart * 10u + static_cast<uint64_t>(*p++ - '0');
    double val = static_cast<double>(intpart);
    if (*p == '.') {
        ++p;
        double frac = 0.1;
        while (static_cast<unsigned>(*p - '0') < 10u) {
            val += static_cast<unsigned>(*p++ - '0') * frac;
            frac *= 0.1;
        }
    }
    return val;
}

static void skip_comma(const char*& p) noexcept { if (*p == ',') ++p; }

static void skip_line(const char*& p) noexcept {
    while (*p && *p != '\n') ++p;
    if (*p == '\n') ++p;
}

} // namespace

Engine::Engine(const char* so_path, const char* csv_path) {
    load_strategy(so_path);
    load_csv(csv_path);
    strategy_->on_init();
}

Engine::~Engine() {
    delete strategy_;
    strategy_ = nullptr;
    if (dl_handle_) { dlclose(dl_handle_); dl_handle_ = nullptr; }
}

void Engine::load_strategy(const char* so_path) {
    dl_handle_ = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if (!dl_handle_)
        throw std::runtime_error(std::string("dlopen: ") + dlerror());

    dlerror();
    void* sym = dlsym(dl_handle_, "create_strategy");
    const char* err = dlerror();
    if (err)
        throw std::runtime_error(std::string("dlsym: ") + err);

    strategy_ = reinterpret_cast<CreateFn>(reinterpret_cast<std::uintptr_t>(sym))();
    if (!strategy_)
        throw std::runtime_error("create_strategy() returned nullptr");

    reclaim_iface_ = dynamic_cast<IReclaimable*>(strategy_);
}

uint8_t Engine::intern_symbol(const char* begin, std::size_t len) {
    for (std::size_t i = 0; i < sym_count_; ++i) {
        if (sym_len_[i] == static_cast<uint8_t>(len) &&
            std::memcmp(sym_buf_[i], begin, len) == 0)
            return static_cast<uint8_t>(i);
    }
    if (sym_count_ >= MAX_SYMBOLS)
        throw std::runtime_error("intern_symbol: exceeded 64 distinct symbols");
    const std::size_t idx = sym_count_++;
    std::memcpy(sym_buf_[idx], begin, len);
    sym_buf_[idx][len] = '\0';
    sym_len_[idx] = static_cast<uint8_t>(len);
    return static_cast<uint8_t>(idx);
}

void Engine::load_csv(const char* csv_path) {
    std::FILE* f = std::fopen(csv_path, "rb");
    if (!f)
        throw std::runtime_error(std::string("cannot open CSV: ") + csv_path);

    std::fseek(f, 0, SEEK_END);
    const long fsz = std::ftell(f);
    if (fsz <= 0) { std::fclose(f); return; }
    std::fseek(f, 0, SEEK_SET);

    std::vector<char> buf(static_cast<std::size_t>(fsz) + 2);
    const std::size_t nread = std::fread(buf.data(), 1,
                                         static_cast<std::size_t>(fsz), f);
    std::fclose(f);
    buf[nread]     = '\n';
    buf[nread + 1] = '\0';

    const char* p = buf.data();
    skip_line(p);

    ticks_.reserve(static_cast<std::size_t>(fsz) / 48 + 1);

    while (*p && *p != '\0') {
        if (*p == '\r' || *p == '\n') { ++p; continue; }

        TickRecord r;
        r.timestamp_ns = parse_u64(p);  skip_comma(p);

        const char* sym_begin = p;
        while (*p && *p != ',') ++p;
        const std::size_t slen = static_cast<std::size_t>(p - sym_begin);
        skip_comma(p);

        r.sym_idx = intern_symbol(sym_begin, slen);
        r.sym_len = static_cast<uint8_t>(slen);

        r.bid_px = parse_price(p); skip_comma(p);
        r.ask_px = parse_price(p); skip_comma(p);
        r.bid_qty = parse_u32(p);  skip_comma(p);
        r.ask_qty = parse_u32(p);

        r._pad[0] = 0; r._pad[1] = 0;

        ticks_.push_back(r);
        skip_line(p);
    }

    ticks_.shrink_to_fit();
}

std::vector<Order> Engine::run_tick(std::size_t idx) {
    const TickRecord& r = ticks_[idx];
    Tick t;
    t.timestamp_ns = r.timestamp_ns;
    t.symbol       = std::string_view{sym_buf_[r.sym_idx], r.sym_len};
    t.bid_px       = r.bid_px;
    t.ask_px       = r.ask_px;
    t.bid_qty      = r.bid_qty;
    t.ask_qty      = r.ask_qty;
    return strategy_->on_tick(t);
}

void Engine::run() {
    const std::size_t n = ticks_.size();
    uint64_t total_orders = 0;

    std::vector<Order> orders;

    for (std::size_t i = 0; i < n; ++i) {
        const TickRecord& r = ticks_[i];

        Tick t;
        t.timestamp_ns = r.timestamp_ns;
        t.symbol       = std::string_view{sym_buf_[r.sym_idx], r.sym_len};
        t.bid_px       = r.bid_px;
        t.ask_px       = r.ask_px;
        t.bid_qty      = r.bid_qty;
        t.ask_qty      = r.ask_qty;

        const auto t0 = std::chrono::steady_clock::now();
        orders        = strategy_->on_tick(t);
        const auto t1 = std::chrono::steady_clock::now();

        hist_.record(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));

        total_orders += orders.size();
        for (const Order& o : orders)
            strategy_->on_fill(o, o.price, o.qty);

        if (reclaim_iface_)
            reclaim_iface_->reclaim_orders(std::move(orders));
    }

    std::cout << "ticks="  << n
              << " orders=" << total_orders << '\n';
    hist_.print(std::cout);
}

} // namespace csot
