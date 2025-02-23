#include <bits/stdc++.h>
#include <cstdint>

#include "Broadcast.cpp"
#include "Constants.h"

using namespace std;
using namespace Constants;


struct ALU_IssueData {
    int operand1;
    int operand1_tag;
    bool operand1_valid;
    int operand2;
    int operand2_tag;
    bool operand2_valid;
    int ALU_op;
    int rob_tag;
};

class ALU {
public:
    // Explicitly mark the operation values to match the SystemVerilog definitions:
    // 0b0000: ADD, 0b0001: SUB, 0b0010: AND, 0b0011: OR,
    // 0b0100: SLT, 0b0101: XOR, 0b0110: NOR, 0b0111: SLL,
    // 0b1000: SRL, 0b1001: SGT.
    enum class Op : int {
        ADD = 0b0000, // Addition
        SUB = 0b0001, // Subtraction
        AND = 0b0010, // Bitwise AND
        OR  = 0b0011, // Bitwise OR
        SLT = 0b0100, // Set on Less Than
        XOR = 0b0101, // Bitwise XOR
        NOR = 0b0110, // Bitwise NOR
        SLL = 0b0111, // Shift Left Logical
        SRL = 0b1000, // Shift Right Logical
        SGT = 0b1001  // Set on Greater Than
    };

    // The compute method calculates the result of applying the given operation on two operands.
    // The result and the zero flag are returned by reference.
    void compute(int operand1, int operand2, Op op,
                 int &result) const {
        switch (op) {
            case Op::ADD: result = operand1 + operand2; break;
            case Op::SUB: result = operand1 - operand2; break;
            case Op::AND: result = operand1 & operand2; break;
            case Op::OR:  result = operand1 | operand2; break;
            case Op::SLT: result = (operand1 < operand2) ? 1 : 0; break;
            case Op::XOR: result = operand1 ^ operand2; break;
            case Op::NOR: result = ~(operand1 | operand2); break;
            case Op::SLL: result = operand2 << (operand1 && 0b11111); break;
            case Op::SRL: result = operand2 >> (operand1 && 0b11111); break;
            case Op::SGT: result = (operand1 > operand2) ? 1 : 0; break;
            default:      result = 0; break;
        }
    }
};

class ALU_RS {
private:
    struct Entry {
        int operand1 = 0;
        bool operand1_valid = false;
        int operand1_tag = 0;
        int operand2 = 0;
        bool operand2_valid = false;
        int operand2_tag = 0;
        int ALU_op = 0;
        bool busy = false;
        int rob_tag = 0;
        int age = 0;
        bool ready = 0;
    };

    vector<Entry> current_entries;
    vector<Entry> next_entries;
    const int NUM_ENTRIES;
    const int AGE_MAX = 3;

    int ready_slot = 0;

public:
    ALU_RS(int NUM_ENTRIES)
        : NUM_ENTRIES(NUM_ENTRIES) {
        current_entries.resize(NUM_ENTRIES);
        next_entries.resize(NUM_ENTRIES);
    }

    void process_inputs(bool issue_in, const ALU_IssueData& issue_data,
                        const Broadcast& broadcast,
                        bool stall = 0, bool flush = 0, bool rst = 0) {
        next_entries = current_entries;
        if (flush || rst) {
            for (auto& entry : next_entries) {
                entry.busy = false;
                entry.ready = false;
                entry.operand1_valid = false;
                entry.operand2_valid = false;
                entry.operand1 = 0;
                entry.operand1_tag = 0;
                entry.operand1_valid = 0;
                entry.operand2 = 0;
                entry.operand2_tag = 0;
                entry.operand2_valid = 0;
                entry.ALU_op = 0;
                entry.rob_tag = 0;
                entry.age = 0;
            }
            return;
        }
        else {
            for(auto& entry: next_entries){
                if(entry.busy && entry.age < AGE_MAX){
                    entry.age++;
                }
            }
            if (issue_in && !stall) {
                int free_slot = find_free_slot();
                auto& entry = current_entries[free_slot];
                entry.operand1 = issue_data.operand1;
                entry.operand1_tag = issue_data.operand1_tag;
                entry.operand1_valid = issue_data.operand1_valid;
                entry.operand2 = issue_data.operand2;
                entry.operand2_tag = issue_data.operand2_tag;
                entry.operand2_valid = issue_data.operand2_valid;
                entry.ALU_op = issue_data.ALU_op;
                entry.rob_tag = issue_data.rob_tag;
                entry.busy = true;
                entry.ready = issue_data.operand1_valid && issue_data.operand2_valid;
                entry.age = 0;
            }
            for (int i = 0; i < NUM_ENTRIES; i++) {
                auto& entry = next_entries[i];
                if (current_entries[i].busy) {
                    if (!current_entries[i].operand1_valid) {
                        for(int i = 0; i < NUM_BROADCASTS; i++) {
                            if(current_entries[i].operand1_tag == broadcast.tags[i] && broadcast.valids[i]){
                                entry.operand1_valid = 1;
                                entry.operand1 = broadcast.results[i];
                            }
                        }   
                    }
                    if (!current_entries[i].operand2_valid) {
                        for(int i = 0; i < NUM_BROADCASTS; i++) {
                            if(current_entries[i].operand2_tag == broadcast.tags[i] && broadcast.valids[i]){
                                entry.operand2_valid = 1;
                                entry.operand2 = broadcast.results[i];
                            }
                        }   
                    }
                    if(entry.operand2_valid && entry.operand2_valid) entry.ready = 1;
                }
            }
    
            calculate_ready_slot();
            if (current_entries[ready_slot].ready) {
                next_entries[ready_slot].busy = false;
                next_entries[ready_slot].ready = false;
                next_entries[ready_slot].operand1_valid = false;
                next_entries[ready_slot].operand2_valid = false;
                next_entries[ready_slot].operand1 = 0;
                next_entries[ready_slot].operand1_tag = 0;
                next_entries[ready_slot].operand2 = 0;
                next_entries[ready_slot].operand2_tag = 0;
                next_entries[ready_slot].ALU_op = 0;
                next_entries[ready_slot].rob_tag = 0;
                next_entries[ready_slot].age = 0;
            }
        }
    }

    void commit_state() { current_entries = next_entries; }


    void get_ready_entry(int& op1, int& op2, int& alu_op, int& rob_tag, bool& valid_out) {
        calculate_ready_slot();
        valid_out = current_entries[ready_slot].ready;
        op1 = current_entries[ready_slot].operand1;
        op2 = current_entries[ready_slot].operand2;
        alu_op = current_entries[ready_slot].ALU_op;
        rob_tag = current_entries[ready_slot].rob_tag;
    }
    void calculate_ready_slot(){
        vector<bool> ready_buffer(NUM_ENTRIES);
        vector<int> age_buffer(NUM_ENTRIES);

        // Compute ready buffer based on operand validity
        for (int i = 0; i < NUM_ENTRIES; i++) {
            ready_buffer[i] = current_entries[i].operand1_valid && current_entries[i].operand2_valid;
            age_buffer[i] = current_entries[i].age;
        }

        // Compute ready_slot01
        int ready_slot01 = ((ready_buffer[0] && (age_buffer[0] >= age_buffer[1])) || !ready_buffer[1]) ? 0 : 1;
        // Compute ready_slot23
        int ready_slot23 = ((ready_buffer[2] && (age_buffer[2] >= age_buffer[3])) || !ready_buffer[3]) ? 2 : 3;
        // Compute final ready_slot
        ready_slot = ((ready_buffer[ready_slot01] && (age_buffer[ready_slot01] >= age_buffer[ready_slot23])) || !ready_buffer[ready_slot23])
                             ? ready_slot01
                             : ready_slot23;
    }

    bool is_full() const {
        for (const auto& entry : current_entries)
            if (!entry.busy) return false;
        return true;
    }

private:
    int find_free_slot() const {
        for (int i = NUM_ENTRIES-1; i >= 0; i--)
            if (!current_entries[i].busy) return i;
        return 0;
    }

};

class ALU_Functional_Unit {
private:
    ALU_RS reservation_station;
    ALU alu;
    bool valid_out = false;
    int result = 0;
    int rob_tag = 0;

public:
    /**
     * Constructor for the ALU_Functional_Unit class.
     * Initializes the reservation station with a specified number of entries.
     *
     * @param num_entries The number of entries for the reservation station.
     */
    ALU_Functional_Unit(int num_entries)
        : reservation_station(num_entries) {}

    void process_inputs(bool issue_in, const ALU_IssueData& issue_data,
                        const Broadcast& broadcast,
                        bool stall, bool flush, bool rst) {
        reservation_station.process_inputs(issue_in, issue_data, broadcast, stall, flush, rst);
    }


    void commit_state() { reservation_station.commit_state(); }



/**
 * Retrieves the outputs of the ALU functional unit.
 *
 * This function determines the current state of the ALU operation:
 * - `valid_out`: A flag indicating whether a valid computation has occurred.
 * - `result_out`: The result of the current ALU computation.
 * - `rob_tag_out`: The reorder buffer tag associated with the current operation.
 *
 * It first checks for a ready entry in the reservation station and computes
 * the result using the ALU if a valid entry is found.
 *
 * @param valid_out Reference to a boolean to store the validity of the output.
 * @param result_out Reference to an int to store the result of the ALU computation.
 * @param rob_tag_out Reference to an int to store the reorder buffer tag.
 */
    void get_outputs(bool& valid_out, int& result_out, int& rob_tag_out) {
        int op1, op2;
        int alu_op;
        reservation_station.get_ready_entry(op1, op2, alu_op, rob_tag_out, valid_out);

        alu.compute(op1, op2, static_cast<ALU::Op>(alu_op), result_out);
    }

    bool is_full() const { return reservation_station.is_full(); }
};