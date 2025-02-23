#include <bits/stdc++.h>
using namespace std;

#include "Broadcast.cpp"
#include "Constants.h"
using namespace std;
using namespace Constants;

// Control Unit Class
class ControlUnit {
private:
    // MIPS Opcodes
    static const int _RType = 0x0, _addi = 0x8, _ori = 0xD, _xori = 0xE, _andi = 0xC,
                    _slti = 0xA, _lw = 0x23, _sw = 0x2b, _beq = 0x4, _bne = 0x5,
                    _jr = 0x8, _jal = 0x3, _j = 0x2;

    // MIPS R-Type function codes
    static const int _add_ = 0x20, _sub_ = 0x22, _and_ = 0x24, _or_ = 0x25,
                    _slt_ = 0x2a, _sgt_ = 0x29, _xor_ = 0x26, _nor_ = 0x27,
                    _sll_ = 0x00, _srl_ = 0x02;



public:
    vector<int> ALU_op;
    vector<bool> invalid_inst;
    bool lsu_type;
    int branch_type;

    ControlUnit() : ALU_op(NUM_ALU, 0b1111), invalid_inst(NUM_SLOTS, false),
                lsu_type(false), branch_type(0) {}

    void process_instruction(const vector<int>& opcode, const vector<int>& funct) {
        // ALU Operations
        for (int a = 0; a < NUM_ALU; a++) {
            invalid_inst[a] = false;
            ALU_op[a] = 0b1111; // Default invalid

            if (opcode[a] == _RType) {
                switch (funct[a]) {
                    case _add_: ALU_op[a] = 0b0000; break;
                    case _sub_: ALU_op[a] = 0b0001; break;
                    case _and_: ALU_op[a] = 0b0010; break;
                    case _or_:  ALU_op[a] = 0b0011; break;
                    case _slt_: ALU_op[a] = 0b0100; break;
                    case _sgt_: ALU_op[a] = 0b1001; break;
                    case _xor_: ALU_op[a] = 0b0101; break;
                    case _nor_: ALU_op[a] = 0b0110; break;
                    case _sll_: ALU_op[a] = 0b0111; break;
                    case _srl_: ALU_op[a] = 0b1000; break;
                    default: invalid_inst[a] = true; break;
                }
            } else if (opcode[a] == _addi || opcode[a] == _ori ||
                       opcode[a] == _xori || opcode[a] == _andi ||
                       opcode[a] == _slti) {
                ALU_op[a] = (opcode[a] == _addi) ? 0b0000 :
                            (opcode[a] == _ori)  ? 0b0011 :
                            (opcode[a] == _xori) ? 0b0101 :
                            (opcode[a] == _andi) ? 0b0010 :
                            (opcode[a] == _slti) ? 0b0100 : 0b1111;
            }
        }

        // Process LSU instruction
        lsu_type = false;
        invalid_inst[2] = false;
        if(opcode[2] == _lw) lsu_type = true;

        // Process Branch instruction
        branch_type = 0;
        invalid_inst[3] = false;
        if(opcode[3] == _RType && funct[3] == _jr) branch_type = 1;
        else if(opcode[3] == _beq) branch_type = 2;
        else if(opcode[3] == _bne) branch_type = 3;
    }
};

// Register File Class
class RegisterFile {
private:

public:
    vector<int> reg_file;
    vector<int> reg_tags;
    vector<bool> reg_valid;
    vector<bool> rob_valid;
    vector<int> read_value1;
    vector<bool> read_value1_valid;
    vector<int> read_value1_tag;
    
    vector<int> read_value2;
    vector<bool> read_value2_valid;
    vector<int> read_value2_tag;

    vector<int> in_order_tags = vector<int>(NUM_SLOTS);
    vector<bool> in_order_reg_wr_en = vector<bool>(NUM_SLOTS);
    vector<int> in_order_dest_reg = vector<int>(NUM_SLOTS);

    Broadcast broadcast = Broadcast();


    bool flush_reg = false;

    vector<bool> commit_wr_en = vector<bool>(ISSUE_WIDTH);
    vector<int> commit_wr_reg = vector<int>(ISSUE_WIDTH);
    vector<int> commit_wr_value = vector<int>(ISSUE_WIDTH);
    vector<int> commit_wr_tag = vector<int>(ISSUE_WIDTH);


    RegisterFile() : 
        reg_file(NUM_REGS),
        reg_tags(NUM_REGS),
        reg_valid(NUM_REGS, true),
        rob_valid(NUM_REGS),
        read_value1(NUM_SLOTS),
        read_value1_valid(NUM_SLOTS),
        read_value1_tag(NUM_SLOTS),
        read_value2(NUM_SLOTS),
        read_value2_valid(NUM_SLOTS),
        read_value2_tag(NUM_SLOTS) {}

    void commit_cycle(
        bool rst,
        bool flush,
        bool stall
    ) {
        if(rst) {
            for(int i = 0; i < NUM_REGS; i++) {
                reg_file[i] <= 0;
                reg_tags[i] <= 0;
                reg_valid[i] <= 1; // Initially, all registers are valid
                rob_valid[i] <= 0;
            }
            flush_reg <= 0;
        }
        else if(flush_reg) {
            flush_reg <= 0;
            for(int i = 0; i < NUM_REGS; i++) {
                reg_tags[i] <= 0;
                reg_valid[i] <= 1; // Initially, all registers are valid
                rob_valid[i] <= 0;
            }
        }
        else {
            flush_reg = flush;

            for(int j = 0; j < ISSUE_WIDTH; j++) {
                if (commit_wr_en[j] && commit_wr_reg[j] != 0) {
                    reg_file[ commit_wr_reg[j] ] = commit_wr_value[j]; // Update register value
                    if(reg_tags[ commit_wr_reg[j] ] == commit_wr_tag[j]) { //only update the valid bits only if the register was waiting for that rob entry
                        reg_tags[commit_wr_reg[j]] = 0; // To prevent invalid broadcasts
                        reg_valid[commit_wr_reg[j]] = 1;      // Mark as valid
                        rob_valid[commit_wr_reg[j]] = 0;      // value is here not in the ROB
                    } //else don't change them - another instruction is still p}ing
                }
            }

            // Update rob_valid based on broadcast tags
            for (int i = 1; i < NUM_REGS; i++) {
                if(!reg_valid[i] && !rob_valid[i]) {
                    if(broadcast.valids[0] && (reg_tags[i] == broadcast.tags[0]) || 
                       broadcast.valids[1] && (reg_tags[i] == broadcast.tags[1]) ||
                        broadcast.valids[2] && (reg_tags[i] == broadcast.tags[2])) {
                        rob_valid[i] = 1;
                    }
                }
            }

            for(int inst = 0; inst < ISSUE_WIDTH; inst++) {
                if(in_order_reg_wr_en[inst] && !stall && in_order_dest_reg[inst] != 0) {
                    reg_tags[in_order_dest_reg[inst]] = in_order_tags[inst];
                    reg_valid[in_order_dest_reg[inst]] = false;
                    rob_valid[in_order_dest_reg[inst]] = false;
                }
            }
        }

        
    }

    void process_read_tags(
        vector<int> read_reg1,
        vector<int> read_reg2,
        vector<int>& read_tag1,
        vector<int>& read_tag2
    ){
        read_tag1.resize(NUM_SLOTS);
        read_tag2.resize(NUM_SLOTS);
        for(int i = 0; i < NUM_SLOTS; i++) {
            read_tag1[i] = reg_tags[ read_reg1[i] ];
            read_tag2[i] = reg_tags[read_reg2[i]];
        }
    }

    
    void process_read_operands(
        const vector<int>& read_reg1,
        const vector<int>& read_reg2,
        const vector<int>& operand1_rob_forward,
        const vector<int>& operand2_rob_forward
    ){
        for(int i = 0; i < NUM_SLOTS; i++) {
            int tag1 = reg_tags[read_reg1[i]];
            int tag2 = reg_tags[read_reg2[i]];

            // Process first operand
            read_value1[i] = 0;
            read_value1_valid[i] = false;
            read_value1_tag[i] = tag1;

            if(reg_valid[read_reg1[i]]) {
                read_value1[i] = reg_file[read_reg1[i]];
                read_value1_valid[i] = true;
            }
            else if(broadcast.valids[0] && tag1 == broadcast.tags[0]) {
                read_value1[i] = broadcast.results[0];
                read_value1_valid[i] = true;
            }
            else if(broadcast.valids[1] && tag1 == broadcast.tags[1]) {
                read_value1[i] = broadcast.results[1];
                read_value1_valid[i] = true;
            }
            else if(broadcast.valids[2] && tag1 == broadcast.tags[2]) {
                read_value1[i] = broadcast.results[2];
                read_value1_valid[i] = true;
            }
            else if(rob_valid[read_reg1[i]]) {
                read_value1[i] = operand1_rob_forward[i];
                read_value1_valid[i] = true;
            }

            // Process second operand (similar logic)
            read_value2[i] = 0;
            read_value2_valid[i] = false;
            read_value2_tag[i] = tag2;

            if(reg_valid[read_reg2[i]]) {
                read_value2[i] = reg_file[read_reg2[i]];
                read_value2_valid[i] = true;
            }
            else if (broadcast.valids[0] && tag2 == broadcast.tags[0]) {
                read_value2[i] = broadcast.results[0];
                read_value2_valid[i] = true;
            }
            else if(broadcast.valids[1] && tag2 == broadcast.tags[1]) {
                read_value2[i] = broadcast.results[1];
                read_value2_valid[i] = true;
            }
            else if(broadcast.valids[2] && tag2 == broadcast.tags[2]) {
                read_value2[i] = broadcast.results[2];
                read_value2_valid[i] = true;
            }  
            if(rob_valid[read_reg2[i]]) {
                read_value2[i] = operand2_rob_forward[i];
                read_value2_valid[i] = true;
            }
        }

    }

    void prcoess_commit_reg_wr(
        vector<bool> commit_wr_en,
        vector<int> commit_wr_reg,
        vector<int> commit_wr_value,
        vector<int> commit_wr_tag
    ) {
        this->commit_wr_en = commit_wr_en;
        this->commit_wr_reg = commit_wr_reg;
        this->commit_wr_value = commit_wr_value;
        this->commit_wr_tag = commit_wr_tag;
    }

    void process_in_order_writes(
        vector<int> in_order_tags,
        vector<bool> in_order_reg_wr_en,
        vector<int> in_order_dest_reg
    ) {
        this->in_order_tags = in_order_tags;
        this->in_order_reg_wr_en = in_order_reg_wr_en;
        this->in_order_dest_reg = in_order_dest_reg;
    }

    void process_broadcast(const Broadcast& broadcast) {
        this->broadcast = broadcast;
    }
};

// Dispatch Stage Class
class DispatchStage {
    private:
    
    // Outputs to the functional units.
    // For each of the NUM_SLOTS we produce an operand value, a valid flag, and a tag.
    public:
        bool rst = 0;
        bool flush = 0;
        bool stall = 0;
        ControlUnit control_unit;
        RegisterFile cur_register_file;
        RegisterFile next_register_file;
        vector<int> operand1_out;
        vector<bool> operand1_valid_out;
        vector<int> operand1_tag_out;
        
        vector<int> operand2_out;
        vector<bool> operand2_valid_out;
        vector<int> operand2_tag_out;
        
        // Immediate output from the instruction.
        vector<int> imm_out;
        
        int branch_type_out = 0;
        int lsu_type_out = 0;
        vector<int> alu_op_out = vector<int>(NUM_ALU);

        vector<int> read_reg1;
        vector<int> read_reg2;

        // Local vectors to hold extracted instruction fields.
        vector<int> opcode, funct, rs, rt, shamt;

        vector<bool> alu_src1, alu_src2;

        vector<bool> operand1_intra_tag_valid;
        vector<int> operand1_intra_tag;
        vector<bool> operand2_intra_tag_valid;
        vector<int> operand2_intra_tag;

        // Invalid instruction flags (one per slot)
        vector<bool> invalid_inst;
        DispatchStage() :
            operand1_out(NUM_SLOTS, 0),
            operand1_valid_out(NUM_SLOTS, false),
            operand1_tag_out(NUM_SLOTS, 0),
            operand2_out(NUM_SLOTS, 0),
            operand2_valid_out(NUM_SLOTS, false),
            operand2_tag_out(NUM_SLOTS, 0),
            imm_out(NUM_SLOTS, 0),
            invalid_inst(NUM_SLOTS, false),
            // Local vectors to hold extracted instruction fields.
            opcode(NUM_SLOTS, 0), funct(NUM_SLOTS, 0),
            rs(NUM_SLOTS, 0), rt(NUM_SLOTS, 0), shamt(NUM_SLOTS, 0)
        {}
    

/**
 * process_cycle_1 is the first half of the dispatch stage's combinational
 * logic. It takes in the instruction words, the valid flags and tags for
 * intra-slot forwarding, and the ALU source select control signals. It
 * extracts the instruction fields and computes the control signals.
 *
 * @param instructions the instruction words for the current cycle
 * @param operand1_intra_tag_valid the valid flags for intra-slot forwarding
 *   of operand 1
 * @param operand1_intra_tag the tags for intra-slot forwarding of operand 1
 * @param operand2_intra_tag_valid the valid flags for intra-slot forwarding
 *   of operand 2
 * @param operand2_intra_tag the tags for intra-slot forwarding of operand 2
 * @param alu_src1 the ALU source select control signals for slots 0 and 1
 * @param alu_src2 the ALU source select control signals for slots 2 and 3
 * @param read_tag1 the tags of the values read from the register file for
 *   operand 1
 * @param read_tag2 the tags of the values read from the register file for
 *   operand 2
 */
    void process_cycle_1(
        const vector<int>& instructions,
        const vector<bool>& operand1_intra_tag_valid,
        const vector<int>& operand1_intra_tag,
        const vector<bool>& operand2_intra_tag_valid,
        const vector<int>& operand2_intra_tag,
        const vector<bool>& alu_src1,  // one per ALU slot (for slots 0 and 1)
        const vector<bool>& alu_src2,  // one per ALU slot (for slots 0 and 1)
        vector<int>& read_tag1,
        vector<int>& read_tag2
    ) {
        this->alu_src1 = alu_src1;
        this->alu_src2 = alu_src2;
        this->operand1_intra_tag_valid = operand1_intra_tag_valid;
        this->operand1_intra_tag = operand1_intra_tag;
        this->operand2_intra_tag_valid = operand2_intra_tag_valid;
        this->operand2_intra_tag = operand2_intra_tag;

        // Extract instruction fields for each valid slot.
        for (int i = 0; i < NUM_SLOTS; i++) {
            opcode[i] = (instructions[i] >> 26) & 0x3F;
            funct[i]  = instructions[i] & 0x3F;
            rs[i]     = (instructions[i] >> 21) & 0x1F;
            rt[i]     = (instructions[i] >> 16) & 0x1F;
            imm_out[i] = instructions[i] & 0xFFFF;  // lower 16 bits
            shamt[i]  = (instructions[i] >> 6) & 0x1F;
        }
        // Process control signals.
        control_unit.process_instruction(opcode, funct);
        invalid_inst = control_unit.invalid_inst;
        branch_type_out = control_unit.branch_type;
        lsu_type_out = control_unit.lsu_type;
        
        // Ask the register file to process the read operands.
        // For this simple simulation, we use rs and rt as the register numbers.
        read_reg1 = rs;
        read_reg2 = rt;
        cur_register_file.process_read_tags(rs, rt, read_tag1, read_tag2);            
    }

/**
 * The second half of the dispatch stage's combinational logic. This function
 * takes in more inputs than the first half, and produces the final outputs
 * to the functional units.
 *
 * @param in_order_tags the tags for in-order writes
 * @param in_order_reg_wr_en the write enables for in-order writes
 * @param in_order_dest_reg the destination register numbers for in-order writes
 * @param commit_wr_en the write enables for commit writes
 * @param commit_wr_reg the destination register numbers for commit writes
 * @param commit_wr_value the values being written by the commit writes
 * @param commit_wr_tag the tags for the commit writes
 * @param operand1_rob_forward the values stored in the ROB for operand 1
 * @param operand2_rob_forward the values stored in the ROB for operand 2
 * @param broadcast the broadcast information for this cycle
 * @param rst whether to reset the register file
 * @param flush whether to flush the register file
 * @param stall whether to stall the register file
 */
    void process_cycle_2(
        const vector<int>& in_order_tags,
        const vector<bool>& in_order_reg_wr_en,
        const vector<int>& in_order_dest_reg,
        const vector<int>& operand1_rob_forward,
        const vector<int>& operand2_rob_forward,
        const Broadcast broadcast,

        bool rst,
        bool flush,
        bool stall
    ){
        this->rst = rst;
        this->flush = flush;
        this->stall = stall;
        cur_register_file.process_broadcast(broadcast);
        cur_register_file.process_in_order_writes(in_order_tags, in_order_reg_wr_en, in_order_dest_reg);
        cur_register_file.process_read_operands(read_reg1, read_reg2, operand1_rob_forward, operand2_rob_forward);
        // For ALU slots (indices 0 and 1)
        for (int j = 0; j < NUM_ALU; j++) {
            // Operand 1: if the alu_src1 flag is set, use the shift amount (zero-extended)
            if (alu_src1[j]) {
                operand1_valid_out[j] = true;
                operand1_out[j] = shamt[j];  // zero-extended shift amount
                operand1_tag_out[j] = 0;
            } else {
                operand1_out[j] = cur_register_file.read_value1[j];
                if (operand1_intra_tag_valid[j]) {
                    operand1_valid_out[j] = false;
                    operand1_tag_out[j] = operand1_intra_tag[j];
                } else {
                    operand1_valid_out[j] = cur_register_file.read_value2_valid[j];
                    operand1_tag_out[j] = cur_register_file.read_value2_tag[j];
                }
            }
            // Operand 2: if alu_src2 is set, use the immediate (sign-extended)
            if (alu_src2[j]) {
                int imm = imm_out[j];
                int ext_imm = (imm & 0x8000) ? (imm | 0xFFFF0000) : imm;
                operand2_out[j] = ext_imm;
                operand2_valid_out[j] = true;
                operand2_tag_out[j] = 0;
            } else {
                operand2_out[j] = cur_register_file.read_value2[j];
                if (operand2_intra_tag_valid[j]) {
                    operand2_valid_out[j] = false;
                    operand2_tag_out[j] = operand2_intra_tag[j];
                } else {
                    operand2_valid_out[j] = cur_register_file.read_value2_valid[j];
                    operand2_tag_out[j] = cur_register_file.read_value2_tag[j];
                }
            }
        }
        
        // For LSU slot (index 2): simply pass the register file outputs.
        {
            int j = 2;
            operand1_out[j] = cur_register_file.read_value1[j];
            operand2_out[j] = cur_register_file.read_value2[j];
            if (operand1_intra_tag_valid[j]) {
                operand1_valid_out[j] = false;
                operand1_tag_out[j] = operand1_intra_tag[j];
            } else {
                operand1_valid_out[j] = cur_register_file.read_value1_valid[j];
                operand1_tag_out[j] = cur_register_file.read_value1_tag[j];
            }
            if (operand2_intra_tag_valid[j]) {
                operand2_valid_out[j] = false;
                operand2_tag_out[j] = operand2_intra_tag[j];
            } else {
                operand2_tag_out[j] = cur_register_file.read_value2_tag[j];
                operand2_valid_out[j] = cur_register_file.read_value2_valid[j];}
        }
        
        // For Branch slot (index 3): similarly use register file outputs.
        {
            int j = 3;
            operand1_out[j] = cur_register_file.read_value1[j];
            operand2_out[j] = cur_register_file.read_value2[j];
            if (operand1_intra_tag_valid[j]) {
                operand1_valid_out[j] = false;
                operand1_tag_out[j] = operand1_intra_tag[j];
            } else {
                operand1_valid_out[j] = cur_register_file.read_value1_valid[j];
                operand1_tag_out[j] = cur_register_file.read_value1_tag[j];
            }

            if (operand2_intra_tag_valid[j]) {
                operand2_valid_out[j] = false;
                operand2_tag_out[j] = operand2_intra_tag[j];
            } else {
                operand2_tag_out[j] = cur_register_file.read_value2_tag[j];
                operand2_valid_out[j] = cur_register_file.read_value2_valid[j];
            }
        }
    }
    void process_commit(
        const vector<bool>& commit_wr_en,
        const vector<int>& commit_wr_reg,
        const vector<int>& commit_wr_value,
        const vector<int>& commit_wr_tag
    ){
        cur_register_file.prcoess_commit_reg_wr(commit_wr_en, commit_wr_reg, commit_wr_value, commit_wr_tag);
    }
    void commit_cycle(){
        cur_register_file.commit_cycle(rst, flush,stall);
    }

    void print_reg_file(ofstream& log_file){
        log_file << "REG VALID | ROB VALID | REGISTER VALUES | REG TAGS"<<endl;
        for(int i = 0; i < 32; i++){
            log_file<<i<<": "<<setw(6)<<cur_register_file.reg_valid[i]<<" | ";
            log_file<<setw(8)<<cur_register_file.rob_valid[i]<<" | ";
            log_file<<setw(12)<<cur_register_file.reg_file[i]<<" | ";
            log_file<<setw(6)<<cur_register_file.reg_tags[i]<<endl;
        }
    }


    };