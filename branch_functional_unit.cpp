#include <bits/stdc++.h>
#include <cstdint>
#include "Broadcast.cpp"
#include "Constants.h"

using namespace std;
using namespace Constants;

struct Branch_IssueData {
    int type;          // Branch type (00: none, 01: jr, 10: beq, 11: bne)
    int operand1;
    int operand1_tag;
    bool operand1_valid;
    int operand2;
    int operand2_tag;
    bool operand2_valid;
    int pc;            // Program Counter value
    int imm;            // Immediate value
    int rob_tag;
    bool branch_prediction; // Prediction from branch predictor
};

class Branch_RS {
private:
    struct Entry {
        int type = 0;
        int operand1 = 0;
        bool operand1_valid = false;
        int operand1_tag = 0;
        int operand2 = 0;
        bool operand2_valid = false;
        int operand2_tag = 0;
        int pc = 0;
        int imm = 0;
        bool branch_prediction = false;
        int rob_tag = 0;
        bool busy = false;
        int age = 0;
        bool ready = 0;
    };

    vector<Entry> current_entries;
    vector<Entry> next_entries;
    const int NUM_ENTRIES;
    const int AGE_MAX = 7;
    int ready_slot = 0;

public:
    Branch_RS(int NUM_ENTRIES)
        : NUM_ENTRIES(NUM_ENTRIES) {
        current_entries.resize(NUM_ENTRIES);
        next_entries.resize(NUM_ENTRIES);
    }

    void process_inputs(bool issue_in, const Branch_IssueData& issue_data,
                       const Broadcast& broadcast,
                       bool stall = false, bool flush = false, bool rst = false) {
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
                entry.type = 0;
                entry.rob_tag = 0;
                entry.age = 0;
            }
            return;
        }
        for(auto& entry: next_entries){
            if(entry.busy && entry.age < AGE_MAX){
                entry.age++;
            }
        }

        // Issue new instruction
        if (issue_in && !stall) {
            int free_slot = find_free_slot();
            auto& entry = next_entries[free_slot];
            entry.type = issue_data.type;
            entry.operand1 = issue_data.operand1;
            entry.operand1_tag = issue_data.operand1_tag;
            entry.operand1_valid = issue_data.operand1_valid;
            entry.operand2 = issue_data.operand2;
            entry.operand2_tag = issue_data.operand2_tag;
            entry.operand2_valid = issue_data.operand2_valid;
            entry.pc = issue_data.pc;
            entry.imm = issue_data.imm;
            entry.rob_tag = issue_data.rob_tag;
            entry.branch_prediction = issue_data.branch_prediction;
            entry.busy = true;
            entry.ready = issue_data.operand1_valid && issue_data.operand2_valid;
        }

        // Update operands from broadcast
        for (int i = 0; i < NUM_ENTRIES; i++) {
            auto& entry = next_entries[i];
            if (current_entries[i].busy) {
                if (!current_entries[i].operand1_valid) {
                    for (int i = 0; i < NUM_BROADCASTS; i++) {
                        if (broadcast.valids[i] && entry.operand1_tag == broadcast.tags[i]) {
                            entry.operand1 = broadcast.results[i];
                            entry.operand1_valid = true;
                        }
                    }
                }
                if (!current_entries[i].operand2_valid) {
                    for (int i = 0; i < NUM_BROADCASTS; i++) {
                        if (broadcast.valids[i] && entry.operand2_tag == broadcast.tags[i]) {
                            entry.operand2 = broadcast.results[i];
                            entry.operand2_valid = true;
                        }
                    }
                }
                if(entry.operand2_valid && entry.operand2_valid) entry.ready = 1;
            }
        }
        calculate_ready_slot();
        // Clear ready slot
        if (current_entries[ready_slot].ready) {
            next_entries[ready_slot].busy = false;
            next_entries[ready_slot].ready = false;
            next_entries[ready_slot].operand1_valid = false;
            next_entries[ready_slot].operand2_valid = false;
            next_entries[ready_slot].operand1 = 0;
            next_entries[ready_slot].operand1_tag = 0;
            next_entries[ready_slot].operand1_valid = 0;
            next_entries[ready_slot].operand2 = 0;
            next_entries[ready_slot].operand2_tag = 0;
            next_entries[ready_slot].operand2_valid = 0;
            next_entries[ready_slot].type = 0;
            next_entries[ready_slot].rob_tag = 0;
            next_entries[ready_slot].age = 0;
        }
    }

    void commit_state() { current_entries = next_entries; }

    void get_ready_entry(int& op1, int& op2, int& type, int& rob_tag, int& imm_out, int& pc, bool& predicted_taken, bool& valid_out) {
        calculate_ready_slot();
        valid_out = current_entries[ready_slot].ready;
        op1 = current_entries[ready_slot].operand1;
        op2 = current_entries[ready_slot].operand2;
        type = current_entries[ready_slot].type;
        rob_tag = current_entries[ready_slot].rob_tag;
        imm_out = current_entries[ready_slot].imm;
        pc = current_entries[ready_slot].pc;
        predicted_taken = current_entries[ready_slot].branch_prediction;
    }
    void print(ofstream& log_file){
        log_file<<"Ready slot: " << ready_slot<<endl;
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
        int ready_slot01 = (ready_buffer[0] && (age_buffer[0] >= age_buffer[1])) || !ready_buffer[1] ? 0 : 1;
        // Compute ready_slot23
        int ready_slot23 = (ready_buffer[2] && (age_buffer[2] >= age_buffer[3])) || !ready_buffer[3] ? 2 : 3;
        // Compute final ready_slot
        ready_slot = (ready_buffer[ready_slot01] && (age_buffer[ready_slot01] >= age_buffer[ready_slot23])) || !ready_buffer[ready_slot23]
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

class Branch_Functional_Unit {
private:
    Branch_RS reservation_station;
    
public:
    bool valid_out = false;
    int next_pc = 0;
    bool branch_taken = false;
    int rob_tag_out = 0;
    bool predicted_taken = false;
    Branch_Functional_Unit(int NUM_ENTRIES)
        : reservation_station(NUM_ENTRIES){}

    void process_inputs(bool issue_in, const Branch_IssueData& issue_data,
                       const Broadcast& broadcast,
                       bool stall, bool flush, bool rst) {
        reservation_station.process_inputs(issue_in, issue_data, broadcast, stall, flush, rst);
    }

    void get_outputs(ofstream& log_file){
        int type;
        int op1, op2;
        int pc_out;
        int imm_out;
        
        reservation_station.get_ready_entry(op1, op2, type, rob_tag_out, imm_out,  pc_out, predicted_taken, valid_out);
        reservation_station.print(log_file);
        // Calculate branch address (PC_out + imm_out + 1)
        int branch_address = pc_out + (imm_out & ((1 << ADDR_WIDTH) - 1)) + 1;
        int pc_plus_one = pc_out + 1;

        if (valid_out) {
            switch (type) {
                case 0b01: // JUMP_REG
                    next_pc = static_cast<int>(op1) & ((1 << ADDR_WIDTH) - 1);
                    branch_taken = true;
                    break;
                case 0b10: // BEQ
                    branch_taken = (op1 == op2);
                    next_pc = branch_taken ? branch_address : pc_plus_one;
                    break;
                
                case 0b11: // BNE
                    branch_taken = (op1 != op2);
                    next_pc = branch_taken ? branch_address : pc_plus_one;
                    break;
                default:   // NO_BRANCH
                    next_pc = pc_plus_one;
                    branch_taken = false;
            }
        }
        else {
            next_pc = pc_plus_one;
            valid_out = false;
            branch_taken = false;
        }
        log_file<<"++++++++++++++"<<endl;
        log_file<<"Valid: "<<valid_out<<endl;
        log_file<<"Operands: ";
        log_file<<op1<<" "<<op2<<endl;
        log_file<<"rob tag out: "<<rob_tag_out<<endl;
        log_file<<"PC plus one: "<<pc_plus_one<<endl;
        log_file<<"Type: "<<type<<endl;
        log_file<<"branch taken: "<<branch_taken<<endl;
        log_file<<"predicted taken: "<<predicted_taken<<endl;
        log_file<<"++++++++++++++"<<endl;
    }


    void commit_state() { reservation_station.commit_state(); }


    bool is_full() const { return reservation_station.is_full(); }
};