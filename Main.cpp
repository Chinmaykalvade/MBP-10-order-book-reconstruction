#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <tuple>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdint>

// Define Price as int64_t for precision with scaling
using Price = int64_t;
// Scale factor for converting double prices to integer representation
constexpr Price PRICE_SCALE = 10000; // Using 10^4 for standard 4dp precision
// Number of price levels to capture for MBP-10
constexpr int LEVELS = 10;

// Structure to hold only the necessary parsed data from an MBO CSV row
struct MboRow {
    std::string ts_recv;
    std::string ts_event;
    char action;
    char side;
    Price price;
    uint64_t size;
    uint64_t order_id;
    // Fields from mbo.csv needed for the final output
    std::string rtype;
    std::string publisher_id;
    std::string instrument_id;
    std::string flags;
    std::string ts_in_delta;
    std::string sequence;
    std::string symbol;
};

// Fast, direct parsing functions to avoid std::stringstream and std::string overhead
namespace FastParser {
    MboRow parse_mbo(const std::string &line) {
        MboRow r{};
        const char* ptr = line.c_str();
        const char* start;

        auto next_field = [&](std::string& out) {
            if (!*ptr) return;
            start = ptr;
            while (*ptr && *ptr != ',') ptr++;
            out.assign(start, ptr - start);
            if (*ptr) ptr++;
        };

        std::string dummy_ts_recv;
        next_field(dummy_ts_recv);
        next_field(r.ts_event);
        r.ts_recv = r.ts_event;

        next_field(r.rtype);
        next_field(r.publisher_id);
        next_field(r.instrument_id);
        
        start = ptr;
        if (*ptr && *ptr != ',') r.action = *ptr;
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr) ptr++;

        start = ptr;
        if (*ptr && *ptr != ',') r.side = *ptr;
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr) ptr++;

        if (*ptr != ',') {
            try {
                r.price = static_cast<Price>(std::stod(ptr) * PRICE_SCALE);
            } catch (const std::invalid_argument& e) {
                r.price = 0;
            }
        }
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr) ptr++;
        
        try {
            r.size = std::stoull(ptr);
        } catch (const std::invalid_argument& e) {
            r.size = 0;
        }
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr) ptr++;

        while (*ptr && *ptr != ',') ptr++;
        if (*ptr) ptr++;

        try {
            r.order_id = std::stoull(ptr);
        } catch (const std::invalid_argument& e) {
            r.order_id = 0;
        }
        while (*ptr && *ptr != ',') ptr++;
        if (*ptr) ptr++;

        next_field(r.flags);
        next_field(r.ts_in_delta);
        next_field(r.sequence);
        next_field(r.symbol);
        
        return r;
    }
}

// [FIXED] Price formatting logic updated for correct rounding.
// Formats a Price value to a string with a maximum of 2 decimal places and no trailing zeros.
// Replaces your existing format_price(...)
std::string format_price(Price px) {
    // MBP wants an empty price field when px==0
    if (px == 0) return "";

    // Convert back to double price
    double d = static_cast<double>(px) / PRICE_SCALE;

    // Format with exactly 2 decimal places
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << d;
    std::string s = ss.str();

    // Strip trailing zeros
    if (auto pos = s.find('.'); pos != std::string::npos) {
        // remove trailing '0'
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        // if it ends in '.', drop it too
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    return s;
}



// Structure representing the Limit Order Book
struct OrderBook {
    std::map<Price, std::pair<uint64_t, uint32_t>, std::greater<>> bids_agg;
    std::map<Price, std::pair<uint64_t, uint32_t>> asks_agg;
    std::unordered_map<uint64_t, std::tuple<Price, uint64_t, char>> active_orders;

    void add(uint64_t id, char side, Price px, uint64_t sz) {
        if (sz == 0) return;
        active_orders[id] = {px, sz, side};
        if (side == 'B') {
            bids_agg[px].first += sz;
            bids_agg[px].second++;
        } else if (side == 'A') {
            asks_agg[px].first += sz;
            asks_agg[px].second++;
        }
    }

    // AFTER: treat `sz` as the number to remove directly
    // AFTER: treat `sz` as the number to remove directly
    void cancel(uint64_t id, uint64_t cancel_sz) {
        auto it_order = active_orders.find(id);
        if (it_order == active_orders.end() || cancel_sz == 0) return;

        auto& [order_px, order_sz, order_side] = it_order->second;

        uint64_t sz_to_remove = std::min(cancel_sz, order_sz);

        // update aggregate
        if (order_side == 'B') {
            auto it_level = bids_agg.find(order_px);
            if (it_level != bids_agg.end()) {
                it_level->second.first -= sz_to_remove;
                it_level->second.second -= (sz_to_remove == order_sz ? 1 : 0);
                if (it_level->second.first == 0) bids_agg.erase(it_level);
            }
        } else {
            auto it_level = asks_agg.find(order_px);
            if (it_level != asks_agg.end()) {
                it_level->second.first -= sz_to_remove;
                it_level->second.second -= (sz_to_remove == order_sz ? 1 : 0);
                if (it_level->second.first == 0) asks_agg.erase(it_level);
            }
        }

        // correctly subtract the cancelled size
        order_sz -= sz_to_remove;
        if (order_sz == 0) {
            active_orders.erase(it_order);
        } else {
            std::get<1>(it_order->second) = order_sz;
        }
    }




    void modify(uint64_t id, char new_side, Price new_px, uint64_t new_sz) {
        auto it_order = active_orders.find(id);
        if (it_order != active_orders.end()) {
            auto& [old_px, old_sz, old_side] = it_order->second;
            if (old_side == 'B') {
                auto it_level = bids_agg.find(old_px);
                if (it_level != bids_agg.end()) {
                    it_level->second.first -= old_sz;
                    it_level->second.second--;
                    if (it_level->second.first == 0) {
                        bids_agg.erase(it_level);
                    }
                }
            } else {
                auto it_level = asks_agg.find(old_px);
                if (it_level != asks_agg.end()) {
                    it_level->second.first -= old_sz;
                    it_level->second.second--;
                    if (it_level->second.first == 0) {
                        asks_agg.erase(it_level);
                    }
                }
            }
        }
        active_orders.erase(id);
        add(id, new_side, new_px, new_sz);
    }
    
    void execute_trade(Price px, uint64_t sz, char aggressor_side) {
        if (sz == 0) return;

        char resting_side = (aggressor_side == 'B') ? 'A' : 'B';
        
        if (resting_side == 'B') {
            auto it_level = bids_agg.find(px);
            if (it_level == bids_agg.end()) return;

            uint64_t traded_size = std::min(it_level->second.first, sz);
            it_level->second.first -= traded_size;

            uint64_t remaining_to_debit = traded_size;
            for (auto it = active_orders.begin(); it != active_orders.end() && remaining_to_debit > 0; ) {
                auto& [order_px, order_sz, order_side] = it->second;
                if (order_px == px && order_side == resting_side) {
                    uint64_t debit_amount = std::min(remaining_to_debit, order_sz);
                    order_sz -= debit_amount;
                    remaining_to_debit -= debit_amount;
                    if (order_sz == 0) {
                        it_level->second.second--;
                        it = active_orders.erase(it);
                        continue;
                    }
                }
                ++it;
            }
            if (it_level->second.first == 0) bids_agg.erase(it_level);
        } else {
            auto it_level = asks_agg.find(px);
            if (it_level == asks_agg.end()) return;

            uint64_t traded_size = std::min(it_level->second.first, sz);
            it_level->second.first -= traded_size;

            uint64_t remaining_to_debit = traded_size;
            for (auto it = active_orders.begin(); it != active_orders.end() && remaining_to_debit > 0; ) {
                auto& [order_px, order_sz, order_side] = it->second;
                if (order_px == px && order_side == resting_side) {
                    uint64_t debit_amount = std::min(remaining_to_debit, order_sz);
                    order_sz -= debit_amount;
                    remaining_to_debit -= debit_amount;
                    if (order_sz == 0) {
                        it_level->second.second--;
                        it = active_orders.erase(it);
                        continue;
                    }
                }
                ++it;
            }
            if (it_level->second.first == 0) asks_agg.erase(it_level);
        }
    }

    void process(const MboRow &r) {
        switch(r.action) {
            case 'A': add(r.order_id, r.side, r.price, r.size); break;
            case 'C': cancel(r.order_id, r.size); break;
            case 'M': modify(r.order_id, r.side, r.price, r.size); break;
            default: break; // Intentionally do nothing for 'T', 'F', etc.
        }
    }

    std::vector<std::string> snapshot() const {
        std::vector<std::string> out;
        out.reserve(LEVELS * 6); 

        auto bid_it = bids_agg.begin();
        auto ask_it = asks_agg.begin();

        for (int i = 0; i < LEVELS; ++i) {
            if (bid_it != bids_agg.end()) {
                out.push_back(format_price(bid_it->first));
                out.push_back(std::to_string(bid_it->second.first));
                out.push_back(std::to_string(bid_it->second.second));
                ++bid_it;
            } else { out.push_back(""); out.push_back("0"); out.push_back("0"); }
            if (ask_it != asks_agg.end()) {
                out.push_back(format_price(ask_it->first));
                out.push_back(std::to_string(ask_it->second.first));
                out.push_back(std::to_string(ask_it->second.second));
                ++ask_it;
            } else { out.push_back(""); out.push_back("0"); out.push_back("0"); }
        }
        return out;
    }

    // inside OrderBook
    int get_depth(uint64_t order_id) const {
        auto it_order = active_orders.find(order_id);
        if (it_order == active_orders.end()) return -1;

        const auto& [px, sz, side] = it_order->second;
        int depth = 0;

        if (side == 'B') {
            for (auto const& [level_px, _] : bids_agg) {
                if (level_px == px) return depth;
                ++depth;
            }
        } else {
            for (auto const& [level_px, _] : asks_agg) {
                if (level_px == px) return depth;
                ++depth;
            }
        }
        return -1;  // not found at all
    }


};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " mbo.csv\n";
        return 1;
    }
    std::ios_base::sync_with_stdio(false);

    std::ifstream in(argv[1]);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file " << argv[1] << std::endl;
        return 1;
    }
    std::ofstream out("output.csv");

    // Header
    out << ",ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence";
    for (int i = 0; i < LEVELS; ++i) {
        out << ",bid_px_" << std::setw(2) << std::setfill('0') << i
            << ",bid_sz_" << std::setw(2) << std::setfill('0') << i
            << ",bid_ct_" << std::setw(2) << std::setfill('0') << i
            << ",ask_px_" << std::setw(2) << std::setfill('0') << i
            << ",ask_sz_" << std::setw(2) << std::setfill('0') << i
            << ",ask_ct_" << std::setw(2) << std::setfill('0') << i;
    }
    out << ",symbol,order_id\n";

    OrderBook book;
    std::string line;
    std::getline(in, line);  // skip header
    size_t row_index = 1;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        MboRow r = FastParser::parse_mbo(line);

        // Skip orphan trades/fills
        if ((r.action == 'T' || r.action == 'F') && r.side == 'N')
            continue;

        // 0) snapshot before any mutation
        auto before_snap = book.snapshot();

        // record old_depth for Cancels
        int old_depth = -1;
        if (r.action == 'C') {
            old_depth = book.get_depth(r.order_id);
        }

        // apply the update for A/M/C
        if (r.action == 'A' || r.action == 'M' || r.action == 'C') {
            book.process(r);
        }

        // record new_depth for Adds/Mods
        int new_depth = -1;
        if (r.action == 'A' || r.action == 'M') {
            new_depth = book.get_depth(r.order_id);
        }

        // 1) snapshot after mutation
        auto after_snap = book.snapshot();

        // 2) decide whether to emit
        bool emit = false;
        char out_act = r.action;
        char out_side = r.side;
        int depth_to_print = 0;

        if (r.action == 'T' || r.action == 'F') {
            // always emit trades
            out_act = 'T';
            out_side = (r.side == 'B') ? 'A' : 'B';
            depth_to_print = 0;
            emit = true;
        }
        else if (r.action == 'C') {
        // emit any cancel at raw depth 0..LEVELS+1 (i.e. 0..11)
            if (old_depth >= 0 && old_depth <= LEVELS + 1) {
                emit = true;
                depth_to_print = old_depth;
            }
        }

        else /* A or M */ {
            // emit adds/modifies that shift the view
            // or that sit exactly at raw depth == LEVELS
            if (before_snap != after_snap || new_depth == LEVELS) {
                emit = true;
                depth_to_print = new_depth;
            }
        }

        if (!emit) continue;

        // 3) write the output row
        out << row_index++ << ","
            << r.ts_recv << "," << r.ts_event << "," << LEVELS << ","
            << r.publisher_id << "," << r.instrument_id << ","
            << out_act << "," << out_side << ","
            << depth_to_print << ",";

        if (r.price != 0) out << format_price(r.price);
        out << "," << r.size << "," << r.flags << ","
            << r.ts_in_delta << "," << r.sequence;

        auto snap = book.snapshot();
        for (auto &f : snap) out << "," << f;

        out << "," << r.symbol << "," << r.order_id << "\n";
    }

    std::cout << "Processing complete. Output written to output.csv\n";
    return 0;
}
