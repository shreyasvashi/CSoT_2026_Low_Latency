#include "strategy.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {

static constexpr int      WINDOW         = 64;
static constexpr double   ENTRY_Z        = 2.0;
static constexpr double   EXIT_Z         = 0.5;
static constexpr double   INV_WINDOW     = 1.0 / 64.0;
static constexpr double   EPSILON_STDDEV = 1e-9;
static constexpr uint32_t ORDER_QTY      = 1u;

struct alignas(64) SymbolState {
    double   mids[WINDOW];
    uint32_t count;
    uint32_t head;
    int32_t  position;
    uint8_t  namelen;
    char     name[15];
};

static_assert(sizeof(SymbolState) % 64 == 0,
              "SymbolState must be a multiple of cache-line size");

class SpecStrategy final : public csot::Strategy {
public:
    void on_init() override {
        sym_count_ = 0;
        std::memset(states_.data(), 0, sizeof(SymbolState) * states_.size());
    }

    std::vector<csot::Order> on_tick(const csot::Tick& t) override {
        SymbolState& st = state_for(t.symbol);

        const double mid = (t.bid_px + t.ask_px) * 0.5;

        st.mids[st.head] = mid;
        st.head = (st.head + 1u) & 63u;
        if (st.count < static_cast<uint32_t>(WINDOW))
            ++st.count;

        if (st.count < static_cast<uint32_t>(WINDOW))
            return {};

        const double* __restrict__ m = st.mids;

        double sum = 0.0;
        for (int j = 0; j < WINDOW; ++j) sum += m[j];
        const double mean = sum * INV_WINDOW;

        double sq = 0.0;
        for (int j = 0; j < WINDOW; ++j) {
            const double d = m[j] - mean;
            sq += d * d;
        }
        const double stddev = std::sqrt(sq * INV_WINDOW);

        if (stddev < EPSILON_STDDEV) return {};

        const double z     = (mid - mean) / stddev;
        const double abs_z = std::abs(z);

        using S = csot::Order::Side;

        if (st.position == 0) {
            if (z >= ENTRY_Z)
                return {csot::Order{S::SELL, t.symbol, t.bid_px, ORDER_QTY}};
            if (z <= -ENTRY_Z)
                return {csot::Order{S::BUY, t.symbol, t.ask_px, ORDER_QTY}};
            return {};
        }

        if (st.position > 0 && abs_z <= EXIT_Z)
            return {csot::Order{S::SELL, t.symbol, t.bid_px,
                                static_cast<uint32_t>(st.position)}};

        if (st.position < 0 && abs_z <= EXIT_Z)
            return {csot::Order{S::BUY, t.symbol, t.ask_px,
                                static_cast<uint32_t>(-st.position)}};

        return {};
    }

    void on_fill(const csot::Order& o,
                 double       /*fill_price*/,
                 uint32_t       fill_qty) override {
        SymbolState& st = state_for(o.symbol);
        if (o.side == csot::Order::Side::BUY)
            st.position += static_cast<int32_t>(fill_qty);
        else
            st.position -= static_cast<int32_t>(fill_qty);
    }

private:
    [[nodiscard]] SymbolState& state_for(std::string_view sym) {
        const auto len = static_cast<uint8_t>(sym.size());
        for (std::size_t i = 0; i < sym_count_; ++i) {
            if (states_[i].namelen == len &&
                std::memcmp(states_[i].name, sym.data(), len) == 0)
                return states_[i];
        }
        if (sym_count_ >= 64u) {
            std::fputs("fatal: SpecStrategy exceeded 64 distinct symbols\n",
                       stderr);
            std::terminate();
        }
        SymbolState& s = states_[sym_count_++];
        std::memset(&s, 0, sizeof(SymbolState));
        std::memcpy(s.name, sym.data(), len);
        s.namelen  = len;
        s.count    = 0;
        s.head     = 0;
        s.position = 0;
        return s;
    }

    alignas(64) std::array<SymbolState, 64> states_;
    std::size_t sym_count_ = 0;
};

} // anonymous namespace

extern "C" csot::Strategy* create_strategy() {
    return new SpecStrategy();
}
