This repository contains a high-performance C++ application designed to reconstruct a 10-level Market-by-Price (MBP-10) limit order book from a Market-by-Order (MBO) event log. The program processes raw, high-frequency financial data from sources like Databento to generate a time-series view of market liquidity.
The primary focus of this project is correctness and speed, implementing custom business logic for trade execution while employing low-latency programming techniques common in quantitative finance.

Key Features
High-Performance C++: Built for speed, using modern C++ for efficient data processing.
Optimized Data Structures: Utilizes std::map for sorted price levels and std::unordered_map for O(1) average-time access to individual orders, ensuring high-speed state management.
Custom Low-Latency Parser: A custom const char* based parser is used for reading the input CSV, significantly outperforming standard library methods like stringstream.
Fixed-Point Arithmetic: Employs scaled 64-bit integers for all price calculations to ensure precision and avoid floating-point overhead.
State-Aware Event Logic: Carefully designed to process events in the correct order and accurately calculate order book depth at the precise moment of each event.

Building and Running the Project
Prerequisites
A C++17 compatible compiler (e.g., g++ or Clang).
A make utility (optional, for convenience).

Compilation
You can compile the project using the following g++ command:
g++ -std=c++17 -O3 -o reconstruction main.cpp
-std=c++17: Enables C++17 features.

-O3: Enables aggressive performance optimizations.
-o reconstruction: Names the output executable reconstruction.

Execution
Run the compiled program from your terminal, passing the path to the input mbo.csv file as a command-line argument.
./reconstruction /path/to/your/mbo.csv
The application will process the input file and generate an output.csv file in the same directory, containing the reconstructed MBP-10 order book snapshots.
