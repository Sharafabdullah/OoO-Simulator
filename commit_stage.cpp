// Load_Store_Functional_Unit.cpp
#include <bits/stdc++.h>
#include <cstdint>
#include "Broadcast.cpp"
#include "Constants.h"
using namespace std;
using namespace Constants;

enum Dispatch_Type { _normal_inst, _jump_reg, _branch, _store };


class CommitStage {
    private:
    
    struct ROBEntry {
        int type;
        int value;
        int dest;
        bool busy;
        bool ready;
        int tag; // Redundant, but for clarity
    };

    struct Internal_Registers {
        vector<int> head = vector<int>(ISSUE_WIDTH);
        vector<int> tail = vector<int>(ISSUE_WIDTH);
        int count = 0;
    };

    
    
    public:
    // ROB state
    vector<ROBEntry> cur_rob;
    vector<ROBEntry> next_rob;
    
    // Pointers and counters
    Internal_Registers cur_internal_reg;
    Internal_Registers next_internal_reg;

    int fetch_count = 0;
    int commit_count = 0;
    vector<bool> commit_valid = vector<bool>(ISSUE_WIDTH);
    bool rob_full = 0;
    vector<int> commit_values = vector<int>(ISSUE_WIDTH);
    vector<int> commit_destinations = vector<int>(ISSUE_WIDTH);
    vector<int> commit_tags = vector<int>(ISSUE_WIDTH);
    vector<bool> commit_actual_takens = vector<bool>(ISSUE_WIDTH);
    vector<int> commit_branch_pcs = vector<int>(ISSUE_WIDTH);
    vector<int> cur_inst_tags_f = vector<int>(ISSUE_WIDTH);

    
    vector<bool> dispatch_en = vector<bool>(ISSUE_WIDTH);
    vector<int> dispatch_type= vector<int>(ISSUE_WIDTH);
    vector<int> dispatch_dest = vector<int>(ISSUE_WIDTH);

    Broadcast broadcast;
    
    bool commit_jump_reg_misprediction = 0;
    bool commit_branch_misprediction = 0;
    int commit_branch_jump_address = 0;
    int commit_branch_jump_address_plus_one = 0;
    int commit_branch_jump_address_plus_two = 0;
    int commit_branch_jump_address_plus_three = 0;

    vector<bool> commit_reg_wr_ens = vector<bool>(ISSUE_WIDTH);
    vector<bool> commit_store_valids = vector<bool>(ISSUE_WIDTH);
    vector<bool> commit_branches = vector<bool>(ISSUE_WIDTH);

    bool broadcast_branch_valid = 0;
    int broadcast_branch_tag = 0;
    int broadcast_branch_result = 0;

    CommitStage()
    {
        cur_rob = vector<ROBEntry>(ROB_SIZE);
        next_rob = vector<ROBEntry>(ROB_SIZE);

        for (auto& entry : cur_rob) {
            entry.busy = false;
            entry.ready = false;
        }
        for(int i = 0; i < ISSUE_WIDTH; i++) {
            cur_internal_reg.head[i] = i + 1;
            cur_internal_reg.tail[i] = i + 1;
        }
        next_internal_reg = cur_internal_reg;
        next_rob = cur_rob;
    }

    void process_cycle(
        bool rst,
        bool flush,
        bool stall
    ) {
        // Prepare next state - after getting the necessary signals ( dispatch, rst, flush, stall,boradcast)
        next_rob = cur_rob;
        next_internal_reg = cur_internal_reg;

        fetch_count = dispatch_en[0] + dispatch_en[1] + dispatch_en[2] + dispatch_en[3];

        auto& count = next_internal_reg.count;
        auto& head = next_internal_reg.head;
        auto& tail = next_internal_reg.tail;

        if (rst || flush) {
            for (auto& entry : next_rob) {
                entry.busy = false;
                entry.ready = false;
                entry.dest = 0;
                entry.tag = 0;
                entry.type = _normal_inst;
                entry.value = 0;
            }
            for(int i = 0; i < ISSUE_WIDTH; i++) {
                head[i] = i + 1;
                tail[i] = i + 1;
            }
            count = 0;
            return;
        }
        // Dispatch new instructions
        for (int i = 0; i < ISSUE_WIDTH; i++) {
            if (dispatch_en[i] && !stall && !next_rob[ tail[i] ].busy) {
                next_rob[ tail[i] ].type = dispatch_type[i];
                next_rob[ tail[i] ].dest = dispatch_dest[i];
                next_rob[ tail[i] ].busy = true;
                next_rob[ tail[i] ].ready = false;
            }
        }

        // Handle broadcast results
        for(int i = 0; i < ROB_SIZE; i++) {
            if(next_rob[i].busy && !next_rob[i].ready){
                for(int j = 0; j < NUM_BROADCASTS; j++) {
                    if(broadcast.valids[j] && broadcast.tags[j] == i){
                        next_rob[i].value = broadcast.results[j];
                        next_rob[i].ready = 1;
                    }
                    if(broadcast_branch_valid && broadcast_branch_tag == i){
                        next_rob[i].value = broadcast_branch_result;
                        next_rob[i].ready = 1;
                    }
                }
            }
        }
        for(int c = 0; c < ISSUE_WIDTH; c++) {
            if(commit_valid[c]) {
                next_rob[ head[c] ].busy <= 0;
            }
        }

        
        for(int i = 0; i < ISSUE_WIDTH; i++) {
            if(commit_count == 1) {
                if(head[i]==(ROB_SIZE-1)) {
                    head[i] = 1;
                }
                else head[i] = head[i] + 1;
            }
            else if(commit_count == 2) {
                if(head[i]==(ROB_SIZE-1)) head[i] = 2;
                else if(head[i]==(ROB_SIZE-2)) head[i] = 1;
                else head[i] = head[i] + 2;
            }
            else if(commit_count == 3){
                if(head[i]==(ROB_SIZE-1)) head[i] = 3;
                else if(head[i]==(ROB_SIZE-2)) head[i] = 2;
                else if(head[i]==(ROB_SIZE-3)) head[i] = 1;
                else head[i] = head[i] + 3;
            }
            else if(commit_count == 4){
                if(head[i]==(ROB_SIZE-1)) head[i] = 4;
                else if(head[i]==(ROB_SIZE-2)) head[i] = 3;
                else if(head[i]==(ROB_SIZE-3)) head[i] = 2;
                else if(head[i]==(ROB_SIZE-4)) head[i] = 1;
                else head[i] = head[i] + 4;
            }


            if(fetch_count == 1) {
                if(tail[i]==(ROB_SIZE-1)) {
                    tail[i] = 1;
                }
                else tail[i] = tail[i] + 1;
            }
            else if(fetch_count == 2) {
                if(tail[i]==(ROB_SIZE-1)) tail[i] = 2;
                else if(tail[i]==(ROB_SIZE-2)) tail[i] = 1;
                else tail[i] = tail[i] + 2;
            }
            else if(fetch_count == 3){
                if(tail[i]==(ROB_SIZE-1)) tail[i] = 3;
                else if(tail[i]==(ROB_SIZE-2)) tail[i] = 2;
                else if(tail[i]==(ROB_SIZE-3)) tail[i] = 1;
                else tail[i] = tail[i] + 3;
            }
            else if(fetch_count == 4){
                if(tail[i]==(ROB_SIZE-1)) tail[i] = 4;
                else if(tail[i]==(ROB_SIZE-2)) tail[i] = 3;
                else if(tail[i]==(ROB_SIZE-3)) tail[i] = 2;
                else if(tail[i]==(ROB_SIZE-4)) tail[i] = 1;
                else tail[i] = tail[i] + 4;
            }
        }
        count = count - commit_count + fetch_count;
    }

    void commit_cycle() {
        cur_rob = next_rob;
        cur_internal_reg = next_internal_reg;
    }
    void update_internal_signals(){
        rob_full = cur_internal_reg.count == (ROB_SIZE - 1) || cur_internal_reg.count == (ROB_SIZE - 2) || (cur_internal_reg.count == ROB_SIZE - 3) || (cur_internal_reg.count == ROB_SIZE - 4);
        
        auto& rob = cur_rob;
        auto& head = cur_internal_reg.head;
        auto& tail = cur_internal_reg.tail;
        auto& count = cur_internal_reg.count;


        vector<int> is_normal_inst(ISSUE_WIDTH);
        vector<int> is_jump_reg(ISSUE_WIDTH);
        vector<int> is_branch(ISSUE_WIDTH);
        vector<int> is_store(ISSUE_WIDTH);
        vector<int> is_branch_misprediction(ISSUE_WIDTH);
        vector<int> is_misprediction(ISSUE_WIDTH);

        for(int i = 0; i < ISSUE_WIDTH; i++) {
            is_normal_inst[i] = (rob[ head[i] ].type == _normal_inst);
            is_jump_reg[i] = (rob[ head[i] ].type == _jump_reg);
            is_branch[i] = (rob[ head[i] ].type == _branch);
            is_store[i] = (rob[ head[i] ].type == _store);

            is_branch_misprediction[i] = is_branch[i] && ( (rob[ head[i] ].value>>31)!= (rob[ head[i] ].value>>30) );


            commit_actual_takens[i] = rob[ head[i] ].value >> 31;

            commit_destinations[i] = rob[ head[i] ].dest & ((1 << ADDR_WIDTH) - 1);
            commit_values[i] = rob[ head[i] ].value;

            commit_branch_pcs[i] = rob[ head[i] ].dest & ((1 << ADDR_WIDTH) - 1); //same as values but truncated
            
            commit_tags[i] = head[i];

            is_misprediction[i] = is_jump_reg[i] || is_branch_misprediction[i];

            cur_inst_tags_f[i] = tail[i];
        }

        if(is_misprediction[0]) {
            commit_branch_jump_address = commit_values[0] & ((1 << ADDR_WIDTH) - 1);
            commit_branch_jump_address_plus_one = commit_values[0] & ((1 << ADDR_WIDTH) - 1) + 1;
            commit_branch_jump_address_plus_two = commit_values[0] & ((1 << ADDR_WIDTH) - 1) + 2;
            commit_branch_jump_address_plus_three = commit_values[0] & ((1 << ADDR_WIDTH) - 1) + 3;
        }
        else if(is_misprediction[1]) {
            commit_branch_jump_address = commit_values[1] & ((1 << ADDR_WIDTH) - 1);
            commit_branch_jump_address_plus_one = commit_values[1] & ((1 << ADDR_WIDTH) - 1) + 1;
            commit_branch_jump_address_plus_two = commit_values[1] & ((1 << ADDR_WIDTH) - 1) + 2;
            commit_branch_jump_address_plus_three = commit_values[1] & ((1 << ADDR_WIDTH) - 1) + 3;
        }
        else if(is_misprediction[2]) { 
            commit_branch_jump_address = commit_values[2] & ((1 << ADDR_WIDTH) - 1);
            commit_branch_jump_address_plus_one = commit_values[2] & ((1 << ADDR_WIDTH) - 1) + 1;
            commit_branch_jump_address_plus_two = commit_values[2] & ((1 << ADDR_WIDTH) - 1) + 2;
            commit_branch_jump_address_plus_three = commit_values[2] & ((1 << ADDR_WIDTH) - 1) + 3;
        }
        else {
            commit_branch_jump_address = commit_values[3] & ((1 << ADDR_WIDTH) - 1);
            commit_branch_jump_address_plus_one = commit_values[3] & ((1 << ADDR_WIDTH) - 1) + 1;
            commit_branch_jump_address_plus_two = commit_values[3] & ((1 << ADDR_WIDTH) - 1) + 2;
            commit_branch_jump_address_plus_three = commit_values[3] & ((1 << ADDR_WIDTH) - 1) + 3;
        }

        if(rob[ head[0] ].busy && rob[ head[0] ].ready) {
            commit_valid[0] = 1;
        }

        if(commit_valid[0] && !is_misprediction[0] && rob[head[1]].busy && rob[head[1]].ready) {
            commit_valid[1] = 1;
        }
        if(commit_valid[1] && !is_misprediction[1] && rob[head[2]].busy && rob[head[2]].ready) {
            commit_valid[2] = 1;
        }
        if(commit_valid[2] && !is_misprediction[2] && rob[head[3]].busy && rob[head[3]].ready) {
            commit_valid[3] = 1;
        }
        commit_count = commit_valid[0] + commit_valid[1] + commit_valid[2] + commit_valid[3];

        for(int o = 0; o < ISSUE_WIDTH; o++) {
            commit_reg_wr_ens[o] = is_normal_inst[o] && commit_valid[o];
            commit_store_valids[o] = is_store[o] && commit_valid[o];
            commit_branches[o] = is_branch[o] && commit_valid[o];
        }

        commit_jump_reg_misprediction = commit_valid[0] && is_jump_reg[0] || commit_valid[1] && is_jump_reg[1] || commit_valid[2] && is_jump_reg[2] || commit_valid[3] && is_jump_reg[3];

        commit_branch_misprediction = commit_valid[0] && is_branch_misprediction[0] || commit_valid[1] && is_branch_misprediction[1] || commit_valid[2] && is_branch_misprediction[2] || commit_valid[3] && is_branch_misprediction[3];
    }

    // Forwarding logic
    void get_forwarding(
        const vector<int>& operand1_tags,
        const vector<int>& operand2_tags,
        vector<int>& operand1_values,
        vector<int>& operand2_values
    ) {
        for (size_t i = 0; i < NUM_SLOTS; ++i) {
            int tag = operand1_tags[i];
            operand1_values[i] = cur_rob[tag].value;
        }
        for (size_t i = 0; i < NUM_SLOTS; ++i) {
            int tag = operand2_tags[i];
            operand2_values[i] = cur_rob[tag].value;
        }
    }

    void print_info(ofstream& log_file){
        log_file<<"Commit ----------------"<<endl;
        log_file<<"ROB Busy: "<<cur_internal_reg.count<<endl;
        log_file<<"Commit Count: "<<commit_count<<endl;
        log_file<<"Fetch Count: "<<fetch_count<<endl;
        log_file<<"Commit Info:"<<endl;
        for(int i = 0; i < ISSUE_WIDTH; i++){
            log_file<<i+1<<"- ";
            log_file<<"Commit Valid: "<<commit_valid[i]<<" | ";
            log_file<<"Commit Tag: "<<cur_internal_reg.head[i] <<" | " ;
            log_file<<"Commit Value: "<<commit_values[i]<<" | ";
            log_file<<"Commit Destination: "<<commit_destinations[i]<<endl;
        }
    }


    void get_branches(
        vector<int>& branch_pcs,
        vector<bool>& branch_actual_takens,
        vector<bool>& commit_branches
    ) {
        branch_pcs = commit_branch_pcs;
        branch_actual_takens = commit_actual_takens;
        commit_branches = commit_branches;
    }

    void process_broadcast(
        const Broadcast& broadcasts,
        const bool broadcast_branch_valid,
        const int broadcast_branch_tag,
        const int broadcast_branch_result
    ) {
        this->broadcast = broadcasts;
        this->broadcast_branch_valid = broadcast_branch_valid;
        this->broadcast_branch_tag = broadcast_branch_tag;
        this->broadcast_branch_result = broadcast_branch_result;
    }

    void process_dispatch(        
        vector<bool> dispatch_en,
        vector<int> dispatch_type,
        vector<int> dispatch_dest){
            
        this->dispatch_en = dispatch_en;
        this->dispatch_type = dispatch_type;
        this->dispatch_dest = dispatch_dest;
    }

    bool get_rob_full(){
        return rob_full = (cur_internal_reg.count == (ROB_SIZE - 1)) || (cur_internal_reg.count == (ROB_SIZE - 2)) || (cur_internal_reg.count == (ROB_SIZE - 3)) || (cur_internal_reg.count == (ROB_SIZE - 4));
    }


};

