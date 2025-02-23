#include "out_of_order_processor.h"

OutOfOrderProcessor::OutOfOrderProcessor(
    const std::string& inst_mem_path, 
    const std::string& data_mem_path
) : 
    PC(0), 
    clk(false), 
    rst(true),
    if_stage(PCbits),
    dispatch_stage(ADDR_WIDTH, DATA_WIDTH, TAG_WIDTH, NUM_REGS),
    alu1(DATA_WIDTH, TAG_WIDTH, 4),
    alu2(DATA_WIDTH, TAG_WIDTH, 4),
    ls_unit(DATA_ADDR, DATA_WIDTH, TAG_WIDTH, 4, 4),
    branch_unit(DATA_WIDTH, TAG_WIDTH, 4),
    commit_stage(DATA_WIDTH, 7, TAG_WIDTH, ADDR_WIDTH)
{
    loadMemory(inst_mem_path, data_mem_path);
    
    // Initialize broadcast signals
    broadcast_valids.fill(false);
    broadcast_results.fill(0);
    broadcast_addresses.fill(0);
    broadcast_tags.fill(0);
}

void OutOfOrderProcessor::loadMemory(
    const std::string& inst_path, 
    const std::string& data_path
) {
    // Load instruction memory from file
    std::ifstream inst_file(inst_path);
    if (!inst_file) {
        throw std::runtime_error("Cannot open instruction memory file");
    }
    
    std::string line;
    while (std::getline(inst_file, line)) {
        instruction_memory.push_back(std::stoul(line, nullptr, 16));
    }

    // Load data memory from file
    std::ifstream data_file(data_path);
    if (!data_file) {
        throw std::runtime_error("Cannot open data memory file");
    }
    
    while (std::getline(data_file, line)) {
        data_memory.push_back(std::stoul(line, nullptr, 16));
    }
}

void OutOfOrderProcessor::runCycles(int num_cycles) {
    // Simulate specified number of cycles
    for (int i = 0; i < num_cycles; ++i) {
        // Toggle clock
        clk = !clk;
        
        // Reset only on first cycle
        if (i == 0) {
            rst = false;
        }

        // Update broadcast bus
        updateBroadcastBus();

        // Update stall and flush signals
        updateStallAndFlushSignals();

        // Run each stage
        if_stage.update(clk, rst, stall, broadcast_misprediction, broadcast_branch_target);
        
        // Continue with other stage updates...
    }
}

void OutOfOrderProcessor::updateBroadcastBus() {
    // Similar to Verilog implementation - populate broadcast signals from functional units
    if (alu1.isValid()) {
        broadcast_valids[0] = true;
        broadcast_results[0] = alu1.getResult();
        broadcast_tags[0] = alu1.getRobTag();
    }

    // Similar logic for other functional units...
}

void OutOfOrderProcessor::updateStallAndFlushSignals() {
    // Compute stall based on functional unit fullness
    stall = (alu1_full && issue_alu1) || 
            (alu2_full && issue_alu2) || 
            (ls_full && issue_ls) || 
            (branch_full && issue_branch) || 
            rob_full;

    // Misprediction handling
    flush = broadcast_misprediction;
}

OutOfOrderProcessor::SimulationMetrics OutOfOrderProcessor::getSimulationMetrics() const {
    SimulationMetrics metrics;
    // Populate metrics from internal tracking
    return metrics;
}

void OutOfOrderProcessor::runToCompletion() {
    // Run until a halt instruction or other termination condition
    while (true) {
        // Add termination conditions here
        runCycles(1);
    }
}