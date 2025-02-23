#include <vector>
#include <cstdint>
#include <bits/stdc++.h>
#include <array>
#include "Constants.h"
using namespace Constants;
using namespace std;


class BranchPredictionUnit {
    public:
        // Parameter constants (matching the Verilog parameters)
        const int TABLE_SIZE = 64;      // Number of entries in the predictor table
        const int TABLE_BITS = 6;       // Number of bits used to index the table
        const int NUM_INSTRUCTIONS = 4; // Number of instructions fetched in parallel
        const int WEAKLY_NOT_TAKEN = 1;   // 2'b01: initial value (weakly not taken)
        const int TAKEN_THRESHOLD = 2;    // Prediction threshold (>= 2 means taken)
    
        // Output: predicted branch outcomes for the fetch stage.
        vector<bool> predicted_branch_taken_f;
    
    private:
        // Internal predictor table: each entry is a two‐bit counter (values 0–3).
        vector<int> predictor_table;
    
    public:
        // Constructor: initialize predictor table and output vector.
        BranchPredictionUnit() {
            predictor_table.resize(TABLE_SIZE, WEAKLY_NOT_TAKEN);
            predicted_branch_taken_f.resize(NUM_INSTRUCTIONS, false);
        }
    
        // Reset method: reinitialize the predictor table to WEAKLY_NOT_TAKEN.
        void reset() {
            std::fill(predictor_table.begin(), predictor_table.end(), WEAKLY_NOT_TAKEN);
        }
    

        /**
         * Commit cycle method for the branch predictor.
         * @param commit_branches boolean vector indicating whether each instruction
         *        is a branch or not.
         * @param commit_actual_takens boolean vector indicating whether each branch
         *        was actually taken or not.
         * @param commit_branch_pcs integer vector of branch PC values.
         * @param rst boolean indicating whether the predictor should be reset.
         */
        void commit_cycle(
                        const vector<bool>& commit_branches, const vector<bool>& commit_actual_takens,
                        const vector<int>& commit_branch_pcs, bool rst) {
            // On reset, reinitialize the predictor table.
            if (rst) {
                reset();
            }
            // --- Sequential Update Logic ---
            // For each commit entry (ISSUE_WIDTH entries), if a branch was committed, update the
            // predictor table using a saturating counter. If the branch was actually taken, increment
            // the counter (up to a maximum of 3); otherwise, decrement (down to a minimum of 0).
            for (int i = 0; i < ISSUE_WIDTH; i++) {
                if (commit_branches[i]) {
                    int index = commit_branch_pcs[i] & ((1 << TABLE_BITS) - 1);
                    if (commit_actual_takens[i]) {
                        if (predictor_table[index] < 3) {  // saturate at 3 (2'b11)
                            predictor_table[index]++;
                        }
                    } else {
                        if (predictor_table[index] > 0) {  // saturate at 0 (2'b00)
                            predictor_table[index]--;
                        }
                    }
                }
            }
        }
        void predict_branch(vector<bool> is_branch, vector<int> fetch_pc, vector<bool>& predicted_branch_taken_f){
            for(int i = 0; i < is_branch.size(); i++){
                int index = fetch_pc[i] & ((1<< TABLE_BITS)-1);
                if(is_branch[i] && predictor_table[index] >= TAKEN_THRESHOLD) predicted_branch_taken_f[i] = 1;
                else predicted_branch_taken_f[i] = 0;
            }
        }
        
    };

class InstMem2Port {
    vector<int> mem = vector<int>(1<<ADDR_WIDTH);
public:
    InstMem2Port() {
        // Read instructions from file
        ifstream file("inst_memory.txt");
        if (!file) {
            throw runtime_error("Cannot open instruction memory file");
        }
        string line;
        int address = 0;
        while (getline(file, line) && address < mem.size()) {
            // Reads a line from the file, converts it to an unsigned long integer (with base 16)
            // and stores it in the memory at the current address. The address is then incremented.
            mem[address++] = stoul(line, nullptr, 16);
        }
    }
    void read(int addr1, int addr2, int& read1, int& read2) {
        // Memory read implementation
        read1 = mem[addr1];
        read2 = mem[addr2];
    }
};

class IF_Stage {
    // Configuration parameters

    // Submodules
    BranchPredictionUnit BPU;
    InstMem2Port IM1, IM2;

    // Internal state
    
public:
    vector<int> instruction_f;
    vector<int> instruction_pc;
    bool alu_turn = false;
    // Outputs
    vector<bool> dispatch_en;
    vector<int> dispatch_type;
    vector<int> dispatch_dest;

    vector<bool> alu_src1_f;
    vector<bool> alu_src2_f;
    vector<bool> operand1_intra_tag_f_valid;
    vector<int> operand1_intra_tag_f;
    vector<bool> operand2_intra_tag_f_valid;
    vector<int> operand2_intra_tag_f;

    vector<int> in_order_tags_d;
    vector<bool> in_order_reg_wr_en_d;
    vector<int> in_order_dest_reg_d;

    vector<bool> instruction_valid_d;
    vector<int> cur_inst_tags_d;
    vector<int> instruction_d;

    vector<bool> alu_src1_d;
    vector<bool> alu_src2_d;
    vector<bool> operand1_intra_tag_d_valid;
    vector<int> operand1_intra_tag_d;
    vector<bool> operand2_intra_tag_d_valid;
    vector<int> operand2_intra_tag_d;


    int branch_pc_d = 0;
    bool predicted_branch_taken_d = false;

    vector<int> opcode;
    vector<int> funct;
    vector<int> rs;
    vector<int> rt;
    vector<int> rd;
    vector<int> destination_reg_f;
    vector<bool> reg_wr_en_f;
    vector<int> imm;

    vector<int> jump_address;
    vector<int> jump_address_plus_one;
    vector<int> jump_address_plus_two;
    vector<int> jump_address_plus_three;
    vector<int> branch_address;
    vector<int> branch_address_plus_one;
    vector<int> branch_address_plus_two;
    vector<int> branch_address_plus_three;
    vector<int> fetch_branch_jump_address;
    vector<int> fetch_branch_jump_address_plus_one;
    vector<int> fetch_branch_jump_address_plus_two;
    vector<int> fetch_branch_jump_address_plus_three;

    vector<bool> instruction_valid_f;
    vector<bool> misprediction_before;
    vector<int> ALU_before;
    vector<bool> lsu_before;
    vector<bool> branch_before;

    vector<bool> is_jump;
    vector<bool> is_jal;
    vector<bool> is_alu = vector<bool>(ISSUE_WIDTH);
    vector<bool> is_lsu;
    vector<bool> is_branch;

    vector<bool> invalid_opcode_f;
    vector<bool> invalid_opcode_d;

    vector<bool> reg_dst;

    vector<bool> predicted_branch_taken_f;

    bool rst = 0;
    bool flush = 0;
    bool stall = 0;

    vector<int> cur_inst_tags_f;

    vector<bool> commit_branches;
    vector<bool> commit_actual_takens;
    vector<int> commit_branch_pcs;
    int commit_branch_jump_address;
    int commit_branch_jump_address_plus_one;
    int commit_branch_jump_address_plus_two;
    int commit_branch_jump_address_plus_three;
    bool commit_branch_misprediction;
    bool commit_jump_reg_misprediction;

    vector<bool> flow_change_active;
    int PC = 0, PC_plus_one = 1, PC_plus_two = 2, PC_plus_three = 3;
    int next_pc, next_pc_plus_one, next_pc_plus_two, next_pc_plus_three;

    IF_Stage(){
        alu_turn = false;
        rst = false;
        flush = false;
        stall = false;
        branch_pc_d = 0;
        predicted_branch_taken_d = false;
        commit_branch_jump_address = 0;
        commit_branch_jump_address_plus_one = 0;
        commit_branch_jump_address_plus_two = 0;
        commit_branch_jump_address_plus_three = 0;
        commit_branch_misprediction = false;
        commit_jump_reg_misprediction = false;

        // Reset PC-related values
        PC = 0;
        PC_plus_one = 1;
        PC_plus_two = 2;
        PC_plus_three = 3;
        next_pc = 0;
        next_pc_plus_one = 1;
        next_pc_plus_two = 2;
        next_pc_plus_three = 3;



        // Initialize vectors with the correct sizes and reset values
        instruction_f.assign(ISSUE_WIDTH, 0);
        instruction_pc.assign(ISSUE_WIDTH, 0);
        destination_reg_f.assign(ISSUE_WIDTH, 0);

        dispatch_en.assign(ISSUE_WIDTH, false);
        dispatch_type.assign(ISSUE_WIDTH, 0);
        dispatch_dest.assign(ISSUE_WIDTH, 0);

        alu_src1_f.assign(ISSUE_WIDTH, false);
        alu_src2_f.assign(ISSUE_WIDTH, false);
        operand1_intra_tag_f.assign(ISSUE_WIDTH, 0);
        operand1_intra_tag_f_valid.assign(ISSUE_WIDTH, false);
        operand2_intra_tag_f.assign(ISSUE_WIDTH, 0);
        operand2_intra_tag_f_valid.assign(ISSUE_WIDTH, false);

        in_order_tags_d.assign(NUM_SLOTS, 0);
        in_order_reg_wr_en_d.assign(NUM_SLOTS, false);
        in_order_dest_reg_d.assign(NUM_SLOTS, 0);

        instruction_valid_d.assign(NUM_SLOTS, false);
        cur_inst_tags_d.assign(NUM_SLOTS, 0);
        instruction_d.assign(NUM_SLOTS, 0);

        alu_src1_d.assign(NUM_SLOTS, false);
        alu_src2_d.assign(NUM_SLOTS, false);
        operand1_intra_tag_d_valid.assign(NUM_SLOTS, false);
        operand1_intra_tag_d.assign(NUM_SLOTS, 0);
        operand2_intra_tag_d_valid.assign(NUM_SLOTS, false);
        operand2_intra_tag_d.assign(NUM_SLOTS, 0);

        opcode.assign(ISSUE_WIDTH, 0);
        funct.assign(ISSUE_WIDTH, 0);
        rs.assign(ISSUE_WIDTH, 0);
        rt.assign(ISSUE_WIDTH, 0);
        rd.assign(ISSUE_WIDTH, 0);
        imm.assign(ISSUE_WIDTH, 0);

        is_jump.assign(ISSUE_WIDTH, false);
        is_jal.assign(ISSUE_WIDTH, false);
        is_branch.assign(ISSUE_WIDTH, false);
        is_alu.assign(ISSUE_WIDTH, false);
        is_lsu.assign(ISSUE_WIDTH, false);
        reg_wr_en_f.assign(ISSUE_WIDTH, false);
        invalid_opcode_f.assign(ISSUE_WIDTH, false);

        reg_dst.assign(ISSUE_WIDTH, false);
        alu_src1_f.assign(ISSUE_WIDTH, false);
        alu_src2_f.assign(ISSUE_WIDTH, false);
        
        instruction_valid_f.assign(ISSUE_WIDTH, false);
        misprediction_before.assign(ISSUE_WIDTH, false);

        ALU_before.assign(ISSUE_WIDTH, 0);
        lsu_before.assign(ISSUE_WIDTH, false);
        branch_before.assign(ISSUE_WIDTH, false);

        predicted_branch_taken_f.assign(ISSUE_WIDTH, false);

        fetch_branch_jump_address.assign(ISSUE_WIDTH, 0);
        fetch_branch_jump_address_plus_one.assign(ISSUE_WIDTH, 0);
        fetch_branch_jump_address_plus_two.assign(ISSUE_WIDTH, 0);
        fetch_branch_jump_address_plus_three.assign(ISSUE_WIDTH, 0);

        branch_address.assign(ISSUE_WIDTH, 0);
        branch_address_plus_one.assign(ISSUE_WIDTH, 0);
        branch_address_plus_two.assign(ISSUE_WIDTH, 0);
        branch_address_plus_three.assign(ISSUE_WIDTH, 0);

        jump_address.assign(ISSUE_WIDTH, 0);
        jump_address_plus_one.assign(ISSUE_WIDTH, 0);
        jump_address_plus_two.assign(ISSUE_WIDTH, 0);
        jump_address_plus_three.assign(ISSUE_WIDTH, 0);

        flow_change_active.assign(ISSUE_WIDTH, false);
        commit_branches.assign(ISSUE_WIDTH, false);
        commit_actual_takens.assign(ISSUE_WIDTH, false);
        commit_branch_pcs.assign(ISSUE_WIDTH, 0);

        IM1.read(0,1,instruction_f[0], instruction_f[1]);
        IM1.read(2,3,instruction_f[2], instruction_f[3]);
    }

    void commit_cycle() {   

        BPU.commit_cycle(commit_branches, commit_actual_takens, commit_branch_pcs, rst );
        if (rst || flush) {
            // Reset handling
            PC = 0;
            PC_plus_one = 1;
            PC_plus_two = 2;
            PC_plus_three = 3;
            for(int j = 0; j < NUM_SLOTS; j++) {
                instruction_valid_d[j] = 0;
                instruction_d[j] = 0;
                operand1_intra_tag_d_valid[j] = 0;
                operand1_intra_tag_d[j] = 0;
    
                operand2_intra_tag_d_valid[j] = 0;
                operand2_intra_tag_d[j] = 0;
    
                cur_inst_tags_d[j] = 0;
                alu_src1_d[j] = 0;
                alu_src2_d[j] = 0;
            }
            predicted_branch_taken_d = 0;
            branch_pc_d = 0;
            alu_turn = 0;
            for(int ind = 0; ind < ISSUE_WIDTH; ind++) {
                in_order_tags_d[ind] = 0;
                in_order_reg_wr_en_d[ind] = 0;
                in_order_dest_reg_d[ind] = 0;
            }
            return;
        }
        if(stall) return;

        for(int ind = 0; ind < ISSUE_WIDTH; ind++) {
            in_order_tags_d[ind] = cur_inst_tags_f[ind];
            in_order_reg_wr_en_d[ind] = reg_wr_en_f[ind] && instruction_valid_f[ind] && (!is_jump[ind]);
            in_order_dest_reg_d[ind] = destination_reg_f[ind];
        }

        PC = next_pc;
        PC_plus_one = next_pc_plus_one;
        PC_plus_two = next_pc_plus_two;
        PC_plus_three = next_pc_plus_three;

        if(is_alu[0]) {
            // Fetch new instruction - if it is JAL change it to addi #31, #0, (instruction_pc + 1)
            if(is_jal[0]){
                int imm = (instruction_pc[0] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ alu_turn ] = instruction_f[0];

            operand1_intra_tag_d[ alu_turn ] = operand1_intra_tag_f[0];
            operand1_intra_tag_d_valid[ alu_turn ] = operand1_intra_tag_f_valid[0];
            operand2_intra_tag_d[ alu_turn ] = operand2_intra_tag_f[0];
            operand2_intra_tag_d_valid[ alu_turn ] = operand2_intra_tag_f_valid[0];
            instruction_valid_d[ alu_turn ] = instruction_valid_f[0];

            cur_inst_tags_d[ alu_turn ] = cur_inst_tags_f[0];

            alu_src1_d[ alu_turn ] = alu_src1_f[0];
            alu_src2_d[ alu_turn ] = alu_src2_f[0];
        }
        else if(is_alu[1] && ALU_before[1]==0){
            if(is_jal[1]){
                int imm = (instruction_pc[1] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ alu_turn ] = instruction_f[1];
    
            operand1_intra_tag_d[ alu_turn ] = operand1_intra_tag_f[1];
            operand1_intra_tag_d_valid[ alu_turn ] = operand1_intra_tag_f_valid[1];
            operand2_intra_tag_d[ alu_turn ] = operand2_intra_tag_f[1];
            operand2_intra_tag_d_valid[ alu_turn ] = operand2_intra_tag_f_valid[1];
            instruction_valid_d[ alu_turn ] = instruction_valid_f[1];
    
            cur_inst_tags_d[ alu_turn ] = cur_inst_tags_f[1];
    
            alu_src1_d[ alu_turn ] = alu_src1_f[1];
            alu_src2_d[ alu_turn ] = alu_src2_f[1];

        }
        else if(is_alu[2] && ALU_before[2]==0){
            if(is_jal[2]){
                int imm = (instruction_pc[2] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ alu_turn ] = instruction_f[2];
    
            operand1_intra_tag_d[ alu_turn ] = operand1_intra_tag_f[2];
            operand1_intra_tag_d_valid[ alu_turn ] = operand1_intra_tag_f_valid[2];
            operand2_intra_tag_d[ alu_turn ] = operand2_intra_tag_f[2];
            operand2_intra_tag_d_valid[ alu_turn ] = operand2_intra_tag_f_valid[2];
            instruction_valid_d[ alu_turn ] = instruction_valid_f[2];
    
            cur_inst_tags_d[ alu_turn ] = cur_inst_tags_f[2];
    
            alu_src1_d[ alu_turn ] = alu_src1_f[2];
            alu_src2_d[ alu_turn ] = alu_src2_f[2];
        }
        else if(is_alu[3] && ALU_before[3]==0){
            if(is_jal[3]){
                int imm = (instruction_pc[3] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ alu_turn ] = instruction_f[3];
    
            operand1_intra_tag_d[ alu_turn ] = operand1_intra_tag_f[3];
            operand1_intra_tag_d_valid[ alu_turn ] = operand1_intra_tag_f_valid[3];
            operand2_intra_tag_d[ alu_turn ] = operand2_intra_tag_f[3];
            operand2_intra_tag_d_valid[ alu_turn ] = operand2_intra_tag_f_valid[3];
            instruction_valid_d[ alu_turn ] = instruction_valid_f[3];
    
            cur_inst_tags_d[ alu_turn ] = cur_inst_tags_f[3];
    
            alu_src1_d[ alu_turn ] = alu_src1_f[3];
            alu_src2_d[ alu_turn ] = alu_src2_f[3];
        }
        else {
            instruction_d[ alu_turn ] = 0;
        
            operand1_intra_tag_d[ alu_turn ] = 0;
            operand1_intra_tag_d_valid[ alu_turn ] = 0;
            operand2_intra_tag_d[ alu_turn ] = 0;
            operand2_intra_tag_d_valid[ alu_turn ] = 0;
            instruction_valid_d[ alu_turn ] = 0;
        
            cur_inst_tags_d[ alu_turn ] = 0;
        
            alu_src1_d[ alu_turn ] = 0;
            alu_src2_d[ alu_turn ] = 0;
        }
        
        
        if(is_alu[1] && ALU_before[1]==1){
            if(is_jal[1]){
                int imm = (instruction_pc[1] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ !alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ !alu_turn ] = instruction_f[1];
    
            operand1_intra_tag_d[ !alu_turn ] = operand1_intra_tag_f[1];
            operand1_intra_tag_d_valid[ !alu_turn ] = operand1_intra_tag_f_valid[1];
            operand2_intra_tag_d[ !alu_turn ] = operand2_intra_tag_f[1];
            operand2_intra_tag_d_valid[ !alu_turn ] = operand2_intra_tag_f_valid[1];
            instruction_valid_d[ !alu_turn ] = instruction_valid_f[1];
    
            cur_inst_tags_d[ !alu_turn ] = cur_inst_tags_f[1];
    
            alu_src1_d[ !alu_turn ] = alu_src1_f[1];
            alu_src2_d[ !alu_turn ] = alu_src2_f[1];

        }
        else if(is_alu[2] && ALU_before[2]==1){
            if(is_jal[2]){
                int imm = (instruction_pc[2] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ !alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ !alu_turn ] = instruction_f[2];
    
            operand1_intra_tag_d[ !alu_turn ] = operand1_intra_tag_f[2];
            operand1_intra_tag_d_valid[ !alu_turn ] = operand1_intra_tag_f_valid[2];
            operand2_intra_tag_d[ !alu_turn ] = operand2_intra_tag_f[2];
            operand2_intra_tag_d_valid[ !alu_turn ] = operand2_intra_tag_f_valid[2];
            instruction_valid_d[ !alu_turn ] = instruction_valid_f[2];
    
            cur_inst_tags_d[ !alu_turn ] = cur_inst_tags_f[2];
    
            alu_src1_d[ !alu_turn ] = alu_src1_f[2];
            alu_src2_d[ !alu_turn ] = alu_src2_f[2];
        }
        else if(is_alu[3] && ALU_before[3]==0){
            if(is_jal[3]){
                int imm = (instruction_pc[3] & ((1 << ADDR_WIDTH) - 1)) + 1;
                instruction_d[ !alu_turn ] = (0b001000 << 26) | 0b00000 << 21 | 0b11111 << 12 |  (imm);
            } else instruction_d[ !alu_turn ] = instruction_f[3];
    
            operand1_intra_tag_d[ !alu_turn ] = operand1_intra_tag_f[3];
            operand1_intra_tag_d_valid[ !alu_turn ] = operand1_intra_tag_f_valid[3];
            operand2_intra_tag_d[ !alu_turn ] = operand2_intra_tag_f[3];
            operand2_intra_tag_d_valid[ !alu_turn ] = operand2_intra_tag_f_valid[3];
            instruction_valid_d[ !alu_turn ] = instruction_valid_f[3];
    
            cur_inst_tags_d[ !alu_turn ] = cur_inst_tags_f[3];
    
            alu_src1_d[ !alu_turn ] = alu_src1_f[3];
            alu_src2_d[ !alu_turn ] = alu_src2_f[3];
        }
        else {
            instruction_d[ !alu_turn ] = 0;
        
            operand1_intra_tag_d[ !alu_turn ] = 0;
            operand1_intra_tag_d_valid[ !alu_turn ] = 0;
            operand2_intra_tag_d[ !alu_turn ] = 0;
            operand2_intra_tag_d_valid[ !alu_turn ] = 0;
            instruction_valid_d[ !alu_turn ] = 0;
        
            cur_inst_tags_d[ !alu_turn ] = 0;
        
            alu_src1_d[ !alu_turn ] = 0;
            alu_src2_d[ !alu_turn ] = 0;
        }



        if(is_lsu[0]) {
            // Fetch new instruction - if it is JAL change it to addi #31, #0, (instruction_pc + 1)
            instruction_d[ 2 ] = instruction_f[0];

            operand1_intra_tag_d[ 2 ] = operand1_intra_tag_f[0];
            operand1_intra_tag_d_valid[ 2 ] = operand1_intra_tag_f_valid[0];
            operand2_intra_tag_d[ 2 ] = operand2_intra_tag_f[0];
            operand2_intra_tag_d_valid[ 2 ] = operand2_intra_tag_f_valid[0];
            instruction_valid_d[ 2 ] = instruction_valid_f[0];

            cur_inst_tags_d[ 2 ] = cur_inst_tags_f[0];
        }
        else if(is_lsu[1] && lsu_before[1]==0){
            instruction_d[ 2 ] = instruction_f[1];
    
            operand1_intra_tag_d[ 2 ] = operand1_intra_tag_f[1];
            operand1_intra_tag_d_valid[ 2 ] = operand1_intra_tag_f_valid[1];
            operand2_intra_tag_d[ 2 ] = operand2_intra_tag_f[1];
            operand2_intra_tag_d_valid[ 2 ] = operand2_intra_tag_f_valid[1];
            instruction_valid_d[ 2 ] = instruction_valid_f[1];
    
            cur_inst_tags_d[ 2 ] = cur_inst_tags_f[1];

        }
        else if(is_lsu[2] && lsu_before[2]==0){
            instruction_d[ 2 ] = instruction_f[2];
    
            operand1_intra_tag_d[ 2 ] = operand1_intra_tag_f[2];
            operand1_intra_tag_d_valid[ 2 ] = operand1_intra_tag_f_valid[2];
            operand2_intra_tag_d[ 2 ] = operand2_intra_tag_f[2];
            operand2_intra_tag_d_valid[ 2 ] = operand2_intra_tag_f_valid[2];
            instruction_valid_d[ 2 ] = instruction_valid_f[2];
    
            cur_inst_tags_d[ 2 ] = cur_inst_tags_f[2];
        }
        else if(is_lsu[3] && lsu_before[3]==0){
            instruction_d[ 2 ] = instruction_f[3];
    
            operand1_intra_tag_d[ 2 ] = operand1_intra_tag_f[3];
            operand1_intra_tag_d_valid[ 2 ] = operand1_intra_tag_f_valid[3];
            operand2_intra_tag_d[ 2 ] = operand2_intra_tag_f[3];
            operand2_intra_tag_d_valid[ 2 ] = operand2_intra_tag_f_valid[3];
            instruction_valid_d[ 2 ] = instruction_valid_f[3];
    
            cur_inst_tags_d[ 2 ] = cur_inst_tags_f[3];
        }
        else {
            instruction_d[ 2 ] = 0;
        
            operand1_intra_tag_d[ 2 ] = 0;
            operand1_intra_tag_d_valid[ 2 ] = 0;
            operand2_intra_tag_d[ 2 ] = 0;
            operand2_intra_tag_d_valid[ 2 ] = 0;
            instruction_valid_d[ 2 ] = 0;
        
            cur_inst_tags_d[ 2 ] = 0;
        }

        //! Branch Slot
        if(is_branch[0]) {
            // Fetch new instruction - if it is JAL change it to addi #31, #0, (instruction_pc + 1)
            instruction_d[ 3 ] = instruction_f[0];

            operand1_intra_tag_d[ 3 ] = operand1_intra_tag_f[0];
            operand1_intra_tag_d_valid[ 3 ] = operand1_intra_tag_f_valid[0];
            operand2_intra_tag_d[ 3 ] = operand2_intra_tag_f[0];
            operand2_intra_tag_d_valid[ 3 ] = operand2_intra_tag_f_valid[0];
            instruction_valid_d[ 3 ] = instruction_valid_f[0];

            cur_inst_tags_d[ 3 ] = cur_inst_tags_f[0];

            predicted_branch_taken_d = predicted_branch_taken_f[0];
            branch_pc_d = instruction_pc[0];
        }
        else if(is_branch[1] && branch_before[1]==0){
            instruction_d[ 3 ] = instruction_f[1];
            
            operand1_intra_tag_d[ 3 ] = operand1_intra_tag_f[1];
            operand1_intra_tag_d_valid[ 3 ] = operand1_intra_tag_f_valid[1];
            operand2_intra_tag_d[ 3 ] = operand2_intra_tag_f[1];
            operand2_intra_tag_d_valid[ 3 ] = operand2_intra_tag_f_valid[1];
            instruction_valid_d[ 3 ] = instruction_valid_f[1];
            
            cur_inst_tags_d[ 3 ] = cur_inst_tags_f[1];
            predicted_branch_taken_d = predicted_branch_taken_f[1];
            branch_pc_d = instruction_pc[1];
        }
        else if(is_branch[2] && branch_before[2]==0){
            instruction_d[ 3 ] = instruction_f[2];
            
            operand1_intra_tag_d[ 3 ] = operand1_intra_tag_f[2];
            operand1_intra_tag_d_valid[ 3 ] = operand1_intra_tag_f_valid[2];
            operand2_intra_tag_d[ 3 ] = operand2_intra_tag_f[2];
            operand2_intra_tag_d_valid[ 3 ] = operand2_intra_tag_f_valid[2];
            instruction_valid_d[ 3 ] = instruction_valid_f[2];
            
            cur_inst_tags_d[ 3 ] = cur_inst_tags_f[2];
            
            predicted_branch_taken_d = predicted_branch_taken_f[2];
            branch_pc_d = instruction_pc[2];
        }
        else if(is_branch[3] && branch_before[3]==0){
            instruction_d[ 3 ] = instruction_f[3];
            
            operand1_intra_tag_d[ 3 ] = operand1_intra_tag_f[3];
            operand1_intra_tag_d_valid[ 3 ] = operand1_intra_tag_f_valid[3];
            operand2_intra_tag_d[ 3 ] = operand2_intra_tag_f[3];
            operand2_intra_tag_d_valid[ 3 ] = operand2_intra_tag_f_valid[3];
            instruction_valid_d[ 3 ] = instruction_valid_f[3];
            
            cur_inst_tags_d[ 3 ] = cur_inst_tags_f[3];
            
            predicted_branch_taken_d = predicted_branch_taken_f[3];
            branch_pc_d = instruction_pc[3];
        }
        else {
            instruction_d[ 3 ] = 0;
        
            operand1_intra_tag_d[ 3 ] = 0;
            operand1_intra_tag_d_valid[ 3 ] = 0;
            operand2_intra_tag_d[ 3 ] = 0;
            operand2_intra_tag_d_valid[ 3 ] = 0;
            instruction_valid_d[ 3 ] = 0;
        
            cur_inst_tags_d[ 3 ] = 0;
        
            predicted_branch_taken_d = 0;
            branch_pc_d = 0;
        }


        alu_turn = !alu_turn;
    }

    void process_cycle(
        bool rst,
        bool stall,
        bool flush,

        const vector<int>& cur_inst_tags_f,

        const vector<bool>& commit_branches,
        const vector<bool>& commit_actual_takens,
        const vector<int>& commit_branch_pcs,

        int commit_branch_jump_address,
        int commit_branch_jump_address_plus_one,
        int commit_branch_jump_address_plus_two,
        int commit_branch_jump_address_plus_three,
        bool commit_branch_misprediction,
        bool commit_jump_reg_misprediction
    ){
        this-> rst = rst;
        this-> stall = stall;
        this-> flush = flush;
        this-> cur_inst_tags_f = cur_inst_tags_f;
        this-> commit_branches = commit_branches;
        this-> commit_actual_takens = commit_actual_takens;
        this-> commit_branch_pcs = commit_branch_pcs;
        this-> commit_branch_jump_address = commit_branch_jump_address;
        this-> commit_branch_jump_address_plus_one = commit_branch_jump_address_plus_one;
        this-> commit_branch_jump_address_plus_two = commit_branch_jump_address_plus_two;
        this-> commit_branch_jump_address_plus_three = commit_branch_jump_address_plus_three;
        this-> commit_branch_misprediction = commit_branch_misprediction;
        this-> commit_jump_reg_misprediction = commit_jump_reg_misprediction;

        // 1. Initialize instruction_pc for each instruction slot.
        instruction_pc[0] = PC;
        instruction_pc[1] = PC_plus_one;
        instruction_pc[2] = PC_plus_two;
        instruction_pc[3] = PC_plus_three;

        IM1.read(instruction_pc[0], instruction_pc[1], instruction_f[0], instruction_f[1]);
        IM1.read(instruction_pc[2], instruction_pc[3], instruction_f[2], instruction_f[3]);

        // 2. For each fetched instruction, decode fields and compute jump addresses.
        for (int i = 0; i < ISSUE_WIDTH; i++) {
            // Extract fields (note: bit‐slicing is done by shifting and masking)
            opcode[i] = (instruction_f[i] >> 26) & 0x3F;  // bits [31:26]
            funct[i]  = instruction_f[i] & 0x3F;          // bits [5:0]
            rs[i]     = (instruction_f[i] >> 21) & 0x1F;  // bits [25:21]
            rt[i]     = (instruction_f[i] >> 16) & 0x1F;  // bits [20:16]
            rd[i]     = (instruction_f[i] >> 11) & 0x1F;  // bits [15:11]
            imm[i]    = instruction_f[i] & 0xFFFF;         // bits [15:0]
            // Compute destination register: if is_jal then 31 else (if reg_dst then rd else rt)
        }
        decode();
        
        // 3. Compute branch addresses.
        // Here we add a constant offset that depends on the instruction slot:
        // for slot 0 add 1, for slot 1 add 2, etc.
        for (int i = 0; i < ISSUE_WIDTH; i++) {
            destination_reg_f[i] = is_jal[i] ? 31 : (reg_dst[i] ? rd[i] : rt[i]);
    
            // For jump addresses, take the lower ADDR_WIDTH bits of the instruction.
            jump_address[i]           = instruction_f[i] & ((1 << ADDR_WIDTH) - 1);
            jump_address_plus_one[i]    = jump_address[i] + 1;
            jump_address_plus_two[i]    = jump_address[i] + 2;
            jump_address_plus_three[i]  = jump_address[i] + 3;

            int add_val = i + 1; // 0->1, 1->2, 2->3, 3->4
            branch_address[i]          = PC         + (imm[i] & ((1 << ADDR_WIDTH) - 1)) + add_val;
            branch_address_plus_one[i] = PC_plus_one  + (imm[i] & ((1 << ADDR_WIDTH) - 1)) + add_val;
            branch_address_plus_two[i] = PC_plus_two  + (imm[i] & ((1 << ADDR_WIDTH) - 1)) + add_val;
            branch_address_plus_three[i] = PC_plus_three + (imm[i] & ((1 << ADDR_WIDTH) - 1)) + add_val;

        }

        // 4. Select the fetch branch/jump address based on jump/branch signals.
        for (int i = 0; i < ISSUE_WIDTH; i++) {
            fetch_branch_jump_address[i] =
                (is_jump[i] || is_jal[i]) ? jump_address[i] : branch_address[i];
            fetch_branch_jump_address_plus_one[i] =
                (is_jump[i] || is_jal[i]) ? jump_address_plus_one[i] : branch_address_plus_one[i];
            fetch_branch_jump_address_plus_two[i] =
                (is_jump[i] || is_jal[i]) ? jump_address_plus_two[i] : branch_address_plus_two[i];
            fetch_branch_jump_address_plus_three[i] =
                (is_jump[i] || is_jal[i]) ? jump_address_plus_three[i] : branch_address_plus_three[i];
        }

        // 5. Instruction valid signals (initially only slot 0 is valid if not stalled).
        instruction_valid_f[0] = !stall;
        instruction_valid_f[1] = false;
        instruction_valid_f[2] = false;
        instruction_valid_f[3] = false;

        // 6. Misprediction signals
        misprediction_before[0] = false;
        misprediction_before[1] = is_jal[0] || is_jump[0] || predicted_branch_taken_f[0];
        misprediction_before[2] = misprediction_before[1] || is_jal[1] || is_jump[1] || predicted_branch_taken_f[1];
        misprediction_before[3] = misprediction_before[2] || is_jal[2] || is_jump[2] || predicted_branch_taken_f[2];

        // 7. ALU, LSU, and branch “before” signals.
        ALU_before[0] = 0;
        lsu_before[0] = 0;
        branch_before[0] = 0;

        ALU_before[1] = is_alu[0] ? 1 : 0;
        lsu_before[1] = is_lsu[0] ? 1 : 0;
        branch_before[1] = is_branch[0] ? 1 : 0;

        ALU_before[2] = is_alu[0] + is_alu[1];
        lsu_before[2] = (is_lsu[1] || is_lsu[0]) ? 1 : 0;
        branch_before[2] = (is_branch[1] || is_branch[0]) ? 1 : 0;

        ALU_before[3] = (is_alu[2] ? 1 : 0) + (is_alu[1] ? 1 : 0) + (is_alu[0] ? 1 : 0);
        lsu_before[3] = (is_lsu[2] || is_lsu[1] || is_lsu[0]) ? 1 : 0;
        branch_before[3] = (is_branch[2] || is_branch[1] || is_branch[0]) ? 1 : 0;

        // 8. Update valid signals for subsequent instructions
        if (instruction_valid_f[0] && !misprediction_before[1] &&
            ( ((lsu_before[1] == 0) && is_lsu[1]) ||
              ((branch_before[1] == 0) && is_branch[1]) ||
              is_alu[1] || is_jump[1] )) {
            instruction_valid_f[1] = true;
        }

        if (instruction_valid_f[1] && !misprediction_before[2] &&
            ( ((lsu_before[2] == 0) && is_lsu[2]) ||
              ((branch_before[2] == 0) && is_branch[2]) ||
              ((ALU_before[2] < NUM_ALU) && is_alu[2]) ||
              is_jump[2] )) {
            instruction_valid_f[2] = true;
        }

        if (instruction_valid_f[2] && !misprediction_before[3] &&
            ( ((lsu_before[3] == 0) && is_lsu[3]) ||
              ((branch_before[3] == 0) && is_branch[3]) ||
              ((ALU_before[3] < NUM_ALU) && is_alu[3]) ||
              is_jump[3] )) {
            instruction_valid_f[3] = true;
        }

        intra_dep_check();
        // Issue to ROB logic
        for (int dd = 0; dd < ISSUE_WIDTH; dd++) {
            dispatch_en[dd] = instruction_valid_f[dd] && !is_jump[dd];
            dispatch_dest[dd] = is_branch[dd] ? instruction_pc[dd] : destination_reg_f[dd];
        }

        bool mispredict_active = commit_branch_misprediction || commit_jump_reg_misprediction;
        bool stall_active = stall;
        BPU.predict_branch(is_branch, instruction_pc, predicted_branch_taken_f);

        for(int i = 0; i < ISSUE_WIDTH; i++){
            flow_change_active[i] = (predicted_branch_taken_f[i] || is_jump[i] || is_jal[i]) && instruction_valid_f[i];
        }
        
        // Next PC computation
        next_pc = mispredict_active ? commit_branch_jump_address :
                  stall_active ? PC :
                  flow_change_active[0] ? fetch_branch_jump_address[0] :
                  flow_change_active[1] ? fetch_branch_jump_address[1] :
                  flow_change_active[2] ? fetch_branch_jump_address[2] :
                  flow_change_active[3] ? fetch_branch_jump_address[3] :
                  (PC + instruction_valid_f[0] + instruction_valid_f[1] + instruction_valid_f[2] + instruction_valid_f[3]);
        
        next_pc_plus_one = mispredict_active ? commit_branch_jump_address_plus_one :
                           stall_active ? PC_plus_one :
                           flow_change_active[0] ? fetch_branch_jump_address_plus_one[0] :
                           flow_change_active[1] ? fetch_branch_jump_address_plus_one[1] :
                           flow_change_active[2] ? fetch_branch_jump_address_plus_one[2] :
                           flow_change_active[3] ? fetch_branch_jump_address_plus_one[3] :
                           (PC_plus_one + instruction_valid_f[0] + instruction_valid_f[1] + instruction_valid_f[2] + instruction_valid_f[3]);
        
        next_pc_plus_two = mispredict_active ? commit_branch_jump_address_plus_two :
                           stall_active ? PC_plus_two :
                           flow_change_active[0] ? fetch_branch_jump_address_plus_two[0] :
                           flow_change_active[1] ? fetch_branch_jump_address_plus_two[1] :
                           flow_change_active[2] ? fetch_branch_jump_address_plus_two[2] :
                           flow_change_active[3] ? fetch_branch_jump_address_plus_two[3] :
                           (PC_plus_two + instruction_valid_f[0] + instruction_valid_f[1] + instruction_valid_f[2] + instruction_valid_f[3]);
        
        next_pc_plus_three = mispredict_active ? commit_branch_jump_address_plus_three :
                             stall_active ? PC_plus_three :
                             flow_change_active[0] ? fetch_branch_jump_address_plus_three[0] :
                             flow_change_active[1] ? fetch_branch_jump_address_plus_three[1] :
                             flow_change_active[2] ? fetch_branch_jump_address_plus_three[2] :
                             flow_change_active[3] ? fetch_branch_jump_address_plus_three[3] :
                             (PC_plus_three + instruction_valid_f[0] + instruction_valid_f[1] + instruction_valid_f[2] + instruction_valid_f[3]);

    }

    void intra_dep_check(){
        // Reset all forwarding tags and valid signals
        for (int dep_ind = 0; dep_ind < ISSUE_WIDTH; dep_ind++) {
            operand1_intra_tag_f[dep_ind] = 0;
            operand1_intra_tag_f_valid[dep_ind] = false;
            operand2_intra_tag_f[dep_ind] = 0;
            operand2_intra_tag_f_valid[dep_ind] = false;
        }
        
        // Check dependencies for instruction 1 on instruction 0
        if (rs[1] == destination_reg_f[0] && reg_wr_en_f[0] && !alu_src1_f[1]) {
            operand1_intra_tag_f_valid[1] = true;
            operand1_intra_tag_f[1] = cur_inst_tags_f[0];
        }
        
        if (rt[1] == destination_reg_f[0] && reg_wr_en_f[0] && !alu_src2_f[1]) {
            operand2_intra_tag_f[1] = cur_inst_tags_f[0];
            operand2_intra_tag_f_valid[1] = true;
        }
        
        // Check dependencies for instruction 2
        if (rs[2] == destination_reg_f[1] && reg_wr_en_f[1] && !alu_src1_f[2]) {
            operand1_intra_tag_f_valid[2] = true;
            operand1_intra_tag_f[2] = cur_inst_tags_f[1];
        }
        else if (rs[2] == destination_reg_f[0] && reg_wr_en_f[0] && !alu_src1_f[2]) {
            operand1_intra_tag_f_valid[2] = true;
            operand1_intra_tag_f[2] = cur_inst_tags_f[0];
        }
        
        if (rt[2] == destination_reg_f[1] && reg_wr_en_f[1] && !alu_src2_f[2]) {
            operand2_intra_tag_f_valid[2] = true;
            operand2_intra_tag_f[2] = cur_inst_tags_f[1];
        }
        else if (rt[2] == destination_reg_f[0] && reg_wr_en_f[0] && !alu_src2_f[2]) {
            operand2_intra_tag_f_valid[2] = true;
            operand2_intra_tag_f[2] = cur_inst_tags_f[0];
        }
        
        // Check dependencies for instruction 3
        if (rs[3] == destination_reg_f[2] && reg_wr_en_f[2] && !alu_src1_f[3]) {
            operand1_intra_tag_f_valid[3] = true;
            operand1_intra_tag_f[3] = cur_inst_tags_f[2];
        }
        else if (rs[3] == destination_reg_f[1] && reg_wr_en_f[1] && !alu_src1_f[3]) {
            operand1_intra_tag_f_valid[3] = true;
            operand1_intra_tag_f[3] = cur_inst_tags_f[1];
        }
        else if (rs[3] == destination_reg_f[0] && reg_wr_en_f[0] && !alu_src1_f[3]) {
            operand1_intra_tag_f_valid[3] = true;
            operand1_intra_tag_f[3] = cur_inst_tags_f[0];
        }
        
        if (rt[3] == destination_reg_f[2] && reg_wr_en_f[2] && !alu_src2_f[3]) {
            operand2_intra_tag_f_valid[3] = true;
            operand2_intra_tag_f[3] = cur_inst_tags_f[2];
        }
        else if (rt[3] == destination_reg_f[1] && reg_wr_en_f[1] && !alu_src2_f[3]) {
            operand2_intra_tag_f_valid[3] = true;
            operand2_intra_tag_f[3] = cur_inst_tags_f[1];
        }
        else if (rt[3] == destination_reg_f[0] && reg_wr_en_f[0] && !alu_src2_f[3]) {
            operand2_intra_tag_f_valid[3] = true;
            operand2_intra_tag_f[3] = cur_inst_tags_f[0];
        }
    }
    
    void decode(){
        // Define opcode and function constants (using hexadecimal notation as in Verilog)
        const int _RType = 0x00;
        const int _addi  = 0x08;
        const int _ori   = 0x0D;
        const int _xori  = 0x0E;
        const int _andi  = 0x0C;
        const int _slti  = 0x0A;
        const int _lw    = 0x23;
        const int _sw    = 0x2B;
        const int _beq   = 0x04;
        const int _bne   = 0x05;
        const int _jr    = 0x08; // For R-Type instructions, funct field value for JR
        const int _jal   = 0x03;
        const int _j     = 0x02;
        
        // R-Type function codes that affect the ALU input
        const int _sll_  = 0x00;
        const int _srl_  = 0x02;
        
        for(int i = 0; i < ISSUE_WIDTH; i++){
            reg_dst[i] = false;
            reg_wr_en_f[i] = true;
            dispatch_type[i] = 0; // 3-bit value; initial value 0
    
            alu_src1_f[i] = false;
            alu_src2_f[i] = false;
    
            is_alu[i] = false;
            is_lsu[i] = false;
            is_branch[i] = false;
            is_jump[i] = false;
            is_jal[i] = false;
            invalid_opcode_f[i] = false;
            // Decode the instruction based on the opcode
            switch (opcode[i]) {
                case _RType:
                    reg_dst[i] = true;
                    if (funct[i] == _jr) {
                        // For JR, we treat it as a branch instruction.
                        is_branch[i] = true;
                        dispatch_type[i] = 0x1;  // binary 001
                        is_alu[i] = false;
                    } else {
                        is_alu[i] = true;
                    }
                    // Special handling for shift instructions.
                    if (funct[i] == _sll_ || funct[i] == _srl_) {
                        alu_src1_f[i] = true;
                    }
                    break;
    
                case _addi:
                case _ori:
                case _xori:
                case _andi:
                case _slti:
                    is_alu[i] = true;
                    alu_src2_f[i] = true;
                    break;
    
                case _lw:
                    is_lsu[i] = true;
                    break;
    
                case _sw:
                    is_lsu[i] = true;
                    reg_wr_en_f[i] = false;
                    dispatch_type[i] = 0x3;  // binary 011
                    break;
    
                case _beq:
                case _bne:
                    is_branch[i] = true;
                    reg_wr_en_f[i] = false;
                    dispatch_type[i] = 0x2;  // binary 010
                    break;
    
                case _j:
                    is_jump[i] = true;
                    break;
    
                case _jal:
                    is_jal[i] = true;
                    is_alu[i] = true;
                    alu_src2_f[i] = true;
                    break;
    
                default:
                    invalid_opcode_f[i] = true;
                    break;
            }
        }
    }
};
