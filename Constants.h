// Constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

// Option 1: Using constexpr (C++11 and later)
namespace Constants {
    constexpr int ADDR_WIDTH = 12;
    constexpr int DATA_WIDTH = 32;
    constexpr int DATA_ADDR = 10;

    constexpr int TAG_WIDTH = 5;
    constexpr int ROB_SIZE = 32;
    
    constexpr int NUM_REGS = 32;
    constexpr int NUM_SLOTS = 4;
    constexpr int ISSUE_WIDTH = 4;
    constexpr int NUM_ALU = 2;
    constexpr int NUM_LSU = 1;
    constexpr int NUM_BRANCH = 1;
    constexpr int BRANCH_LS_UNITS = 2;
    constexpr int NUM_BROADCASTS = 3;
    // Add more constants as needed.
}


#endif // CONSTANTS_H