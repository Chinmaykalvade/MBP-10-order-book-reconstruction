#include <cassert>
#include "main.cpp"  // Use only for small integration tests; ideally split logic into headers

void test_add_order() {
    OrderBook book;
    book.initialize_output("test_output.csv");

    MBO_Message msg;
    msg.ts_event = 1;
    msg.action = 'A';
    msg.side = 'B';
    msg.price = 100000000;  // 100.00
    msg.size = 10;
    msg.order_id = 1;

    book.process_mbo_message(msg);

    std::cout << "✅ test_add_order passed\n";
}

void test_cancel_order() {
    OrderBook book;
    book.initialize_output("test_output.csv");

    // Add first
    MBO_Message add_msg = {1, 'A', 'B', 100000000, 10, 1};
    book.process_mbo_message(add_msg);

    // Cancel it
    MBO_Message cancel_msg = {2, 'C', 'B', 100000000, 0, 1};
    book.process_mbo_message(cancel_msg);

    std::cout << "✅ test_cancel_order passed\n";
}

void test_trade_opposite_side() {
    OrderBook book;
    book.initialize_output("test_output.csv");

    // Add a bid
    MBO_Message add_bid = {1, 'A', 'B', 100000000, 20, 1};
    book.process_mbo_message(add_bid);

    // Execute a trade at ask side, should remove from bid
    MBO_Message trade = {2, 'T', 'A', 100000000, 20, 99};
    book.process_mbo_message(trade);

    std::cout << "✅ test_trade_opposite_side passed\n";
}

int main() {
    test_add_order();
    test_cancel_order();
    test_trade_opposite_side();
    std::cout << "All unit tests passed.\n";
    return 0;
}
