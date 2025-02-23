#include <iostream>
#include <bits/stdc++.h>
using namespace std;

#include "fetch_stage.cpp"
#include "Dispatch_Stage.cpp"
#include "alu_functional_unit.cpp"
#include "branch_functional_unit.cpp"
#include "load_store_functional_unit.cpp"
#include "commit_stage.cpp"
#include "Broadcast.cpp"


#define all(v)        ((v).begin()), ((v).end())
#define rep(i, a, b) for (int i = a; i < b; ++i)
#define repd(i, a, b) for (int i = a; i >= b; --i)
#define pb            push_back
#define B             begin()
#define E             end()
#define clr(x)        memset(x,0,sizeof(x))
#define endl          '\n'
#define FASTIO ios::sync_with_stdio(0),cin.tie(0),log_file.tie(0)

typedef long long ll;
typedef unsigned long long ull;
typedef long double   ld;
typedef pair<int, int> pi;
typedef vector<bool>      vb;
typedef vector<vb>        vvb;
typedef vector<string>    vs;
typedef vector<int>       vi;
typedef vector<ll>       vll;
typedef vector<double>    vd;
typedef vector< vi >      vvi;

#ifndef ONLINE_JUDGE
#define deb(...) cerr << "[" << #__VA_ARGS__ << "] = "; _print(__VA_ARGS__); cerr << endl;
#else
#define deb(...)
#endif

void _print(ll t) {cerr << t;}
void _print(int t) {cerr << t;}
void _print(bool t) {cerr << t;}
void _print(string t) {cerr << t;}
void _print(char t) {cerr << t;}
void _print(ld t) {cerr << t;}
void _print(double t) {cerr << t;}
void _print(ull t) {cerr << t;}


template <class T, class V> void _print(pair <T, V> p) {cerr << "{"; _print(p.first); cerr << ","; _print(p.second); cerr << "}";}
template <class T> void _print(vector <T> v) {cerr << "[ "; for (T i : v) {_print(i); cerr << " ";} cerr << "]";}
template <class T> void _print(set <T> v) {cerr << "[ "; for (T i : v) {_print(i); cerr << " ";} cerr << "]";}
template <class T, class V> void _print(map <T, V> v) {cerr << "[ "; for (auto i : v) {_print(i); cerr << " ";} cerr << "]";}
template <class T> void _print(multiset <T> v) {cerr << "[ "; for (T i : v) {_print(i); cerr << " ";} cerr << "]";}

template <typename T, typename... Args>
void _print(T t, Args... args) {_print(t);cerr << ", ";_print(args...);}


ofstream log_file("./log files/execution_log.txt");

class Processor {
private:

public:
    IF_Stage fetch;
    DispatchStage dispatch;
    ALU_Functional_Unit alu0;
    ALU_Functional_Unit alu1;
    Branch_Functional_Unit branch_unit;
    Load_Store_Functional_Unit lsu;
    CommitStage commit;

    Broadcast broadcast;
    bool broadcast_branch_valid = 0;
    int broadcast_branch_result = 0;
    int broadcast_branch_tag = 0;



    bool stall = false;
    bool flush = false;
    bool rst = false;
    Processor()
        : alu0(4),
          alu1(4),
          branch_unit(4),
          lsu(4, 4, 0xfff, 5) {}

    void process_cycle() {
        vector<bool> commit_branches(ISSUE_WIDTH);
        vector<bool> commit_actual_takens(ISSUE_WIDTH);
        vector<int> commit_branch_pcs(ISSUE_WIDTH);

        commit.update_internal_signals();

        commit.get_branches(commit_branch_pcs, commit_actual_takens, commit_branches);

        flush = commit.commit_branch_misprediction | commit.commit_jump_reg_misprediction;

        log_file<<dec<<"PC: "<<fetch.PC<<endl;

        log_file << "Commit Branch Signals: "<<endl;
        for (int i = 0; i < ISSUE_WIDTH; i++){
            log_file << "branch pcs: " << commit_branch_pcs[i]
                 << "| actual takens: " << commit_actual_takens[i]
                 << "| commit_branch: " << commit_branches[i] << endl;
        }
        log_file << endl;

        flush = commit.commit_branch_misprediction | commit.commit_jump_reg_misprediction;
        stall = commit.get_rob_full() || alu0.is_full() && fetch.instruction_d[0] || alu1.is_full() && fetch.instruction_d[1]  || lsu.is_full() && fetch.instruction_d[2] || branch_unit.is_full() && fetch.instruction_d[3];

        log_file << "_______________"<<endl;
        log_file << "Flush Signal: " << flush << endl;
        log_file << "Stall Signal: " << stall << endl;

        commit.update_internal_signals();
        // log_file<<"Commit Internal Register Tail 0: "<<commit.cur_internal_reg.tail[0]<<endl;
        // log_file<<"Commit Internal Register Tail 1: "<<commit.cur_internal_reg.tail[1]<<endl;
        // log_file<<"Commit Internal Register Tail 2: "<<commit.cur_internal_reg.tail[2]<<endl;
        // log_file<<"Commit Internal Register Tail 3: "<<commit.cur_internal_reg.tail[3]<<endl;
        // log_file<<"--------------"<<endl;
        
        fetch.process_cycle(
            rst,
            stall,
            flush, 
            commit.cur_inst_tags_f, 
            commit_branches,
            commit_actual_takens, 
            commit_branch_pcs,
            commit.commit_branch_jump_address,
            commit.commit_branch_jump_address_plus_one,
            commit.commit_branch_jump_address_plus_two,
            commit.commit_branch_jump_address_plus_three,
            commit.commit_branch_misprediction,
            commit.commit_jump_reg_misprediction
        );

        commit.process_dispatch(fetch.dispatch_en, fetch.dispatch_type, fetch.dispatch_dest);
        
        print_fetch_dispatch_signals();

        vector<int> operand1_rob_forward(NUM_SLOTS); 
        vector<int> operand2_rob_forward(NUM_SLOTS); 
        vector<int> read_tag1(NUM_SLOTS);
        vector<int> read_tag2(NUM_SLOTS);
        dispatch.process_cycle_1(
            fetch.instruction_d,
            fetch.operand1_intra_tag_d_valid,
            fetch.operand1_intra_tag_d,
            fetch.operand2_intra_tag_d_valid,
            fetch.operand2_intra_tag_d,
            fetch.alu_src1_d,
            fetch.alu_src2_d,
            read_tag1,
            read_tag2);

        commit.get_forwarding(read_tag1, read_tag2, operand1_rob_forward, operand2_rob_forward);

        dispatch.process_cycle_2(
            fetch.in_order_tags_d,
            fetch.in_order_reg_wr_en_d,
            fetch.in_order_dest_reg_d,
            operand1_rob_forward,
            operand2_rob_forward,
            broadcast,
            rst, flush, stall);

        print_issue_data();

        // Issue to alu0
        vector<ALU_IssueData> alu_issue_data(NUM_ALU);
        for(int i = 0; i < NUM_ALU; i++){
            alu_issue_data[i].ALU_op = dispatch.alu_op_out[i];
            alu_issue_data[i].operand1 = dispatch.operand1_out[i];
            alu_issue_data[i].operand1_tag = dispatch.operand1_tag_out[i];
            alu_issue_data[i].operand1_valid = dispatch.operand1_valid_out[i];
            alu_issue_data[i].operand2 = dispatch.operand2_out[i];
            alu_issue_data[i].operand2_tag = dispatch.operand2_tag_out[i];
            alu_issue_data[i].operand2_valid = dispatch.operand2_valid_out[i];
            alu_issue_data[i].rob_tag = fetch.cur_inst_tags_d[i];
        }

        alu0.process_inputs(fetch.instruction_valid_d[0], alu_issue_data[0], broadcast, stall, flush, rst);
        alu1.process_inputs(fetch.instruction_valid_d[1], alu_issue_data[1], broadcast, stall, flush, rst);

        
        
        bool alu0_valid, alu1_valid;
        int alu0_result, alu1_result;
        int alu0_rob_tag, alu1_rob_tag;


        alu0.get_outputs(alu0_valid, alu0_result, alu0_rob_tag);
        alu1.get_outputs(alu1_valid, alu1_result, alu1_rob_tag);

        //!----------------------------- Issue to Load/Store Unit ----------------------------
        LSQ_IssueData lsu_issue_data;
        lsu_issue_data.base_reg_tag = dispatch.operand1_tag_out[2];
        lsu_issue_data.base_reg_valid = dispatch.operand1_valid_out[2];
        lsu_issue_data.base_reg_value = dispatch.operand1_out[2];

        lsu_issue_data.store_value = dispatch.operand2_out[2];
        lsu_issue_data.store_value_tag = dispatch.operand2_tag_out[2];
        lsu_issue_data.store_value_valid = dispatch.operand2_valid_out[2];

        lsu_issue_data.imm_value = dispatch.imm_out[2];
        lsu_issue_data.rob_tag = fetch.cur_inst_tags_d[2];
        lsu_issue_data.type = dispatch.lsu_type_out;
    
        lsu.process_inputs(fetch.instruction_valid_d[2], lsu_issue_data, broadcast, commit.commit_store_valids, commit.commit_tags, stall, flush, rst);
        
        //!----------------------------- Issue to Branch Unit ----------------------------
        // Issue to branch_unit Unit
        Branch_IssueData branch_issue_data;
        branch_issue_data.operand1 = dispatch.operand1_out[3];
        branch_issue_data.operand1_tag = dispatch.operand1_tag_out[3];
        branch_issue_data.operand1_valid = dispatch.operand1_valid_out[3];
        branch_issue_data.operand2 = dispatch.operand2_out[3];
        branch_issue_data.operand2_tag = dispatch.operand2_tag_out[3];
        branch_issue_data.operand2_valid = dispatch.operand2_valid_out[3];
        branch_issue_data.type = dispatch.branch_type_out;
        branch_issue_data.imm = dispatch.imm_out[3];
        branch_issue_data.pc = fetch.branch_pc_d;
        branch_issue_data.rob_tag = fetch.cur_inst_tags_d[3];
        branch_issue_data.branch_prediction = fetch.predicted_branch_taken_d;
        
        branch_unit.process_inputs(fetch.instruction_valid_d[3], branch_issue_data, broadcast, stall, flush, rst);
        branch_unit.get_outputs(log_file);
        
        //! --------------------------------Broadcasts-------------------------------------
        broadcast.results[0] = alu0_result;
        broadcast.results[1] = alu1_result; 
        broadcast.tags[0] = alu0_rob_tag;
        broadcast.tags[1] = alu1_rob_tag;
        broadcast.valids[0] = alu0_valid;
        broadcast.valids[1] = alu1_valid;

        broadcast.results[2] = lsu.data_out;
        broadcast.tags[2] = lsu.tag_out;
        broadcast.valids[2] = lsu.valid_out;
        
        broadcast_branch_valid = branch_unit.valid_out;
        broadcast_branch_result = (branch_unit.branch_taken <<31) | (branch_unit.predicted_taken<<30) | (branch_unit.next_pc);
        broadcast_branch_tag = branch_unit.rob_tag_out;
        
        print_broadcast();

        //!----------------------------- Issue to Commit Unit ----------------------------
        commit.process_broadcast(broadcast, broadcast_branch_valid, broadcast_branch_tag, broadcast_branch_result);
        commit.process_cycle(rst, flush, stall);

        commit.print_info(log_file);

        
        dispatch.process_commit(commit.commit_reg_wr_ens,
                                commit.commit_destinations,
                                commit.commit_values,
                                commit.commit_tags);


        // Update pipeline state
        fetch.commit_cycle();
        alu0.commit_state();
        alu1.commit_state();
        branch_unit.commit_state();
        dispatch.commit_cycle();
        lsu.commit_cycle();
        commit.commit_cycle();

        // dispatch.print_reg_file(log_file);
    }

    void print_fetch_dispatch_signals(){
        log_file<<"Valids_f |  instruction_f | and tags_f: "<<endl;
        for(int i = 0; i < NUM_SLOTS; i++){
            log_file<<setw(5)<<fetch.instruction_valid_f[i]<<" | "<<setw(15)<<hex<<fetch.instruction_f[i]<<setw(15)<<dec<<" "<<fetch.cur_inst_tags_f[i]<<endl;
        }

        // log_file<<"Fetch Stage Destination: "<<endl;
        // for(int i = 0; i < NUM_SLOTS; i++){
        //     log_file<<"destination_reg_f: "<<fetch.destination_reg_f[i]<<endl;
        // }
        
        log_file<<"___________________"<<endl;
        log_file<<"Dispatch Info(from fetch to commit):"<<endl;
        for(int i = 0; i < ISSUE_WIDTH;i++){
            log_file<<"Valid: "<<fetch.dispatch_en[i]<<" | Type: "<<fetch.dispatch_type[i]<<" | Destination:"<<fetch.dispatch_dest[i]<<endl;
        }

        log_file<<"In order writes, destination: "<<endl;
        for(int i = 0; i < NUM_SLOTS; i++){
            log_file<<"Dest: "<<fetch.in_order_dest_reg_d[i]<<" | Wr En: "<<fetch.in_order_reg_wr_en_d[i]<<" Tags:"<<fetch.in_order_tags_d[i]<<endl;
        }
        
        // log_file<<"OoO Valids, instruction_d, and tags: "<<endl;
        // for(int i = 0; i < NUM_SLOTS; i++){
        //     log_file<<fetch.instruction_valid_d[i]<<" "<<setw(8)<<hex<<fetch.instruction_d[i]<<dec<<" "<<fetch.cur_inst_tags_d[i]<<endl;
        // }
    }

    void print_issue_data(){
        log_file<<endl;
        log_file<<"ISSUE DATA---------------"<<endl;
        log_file<<"ALU0: ";
        log_file <<"Issue Valid: "<<fetch.instruction_valid_d[0]<< " | Tag: "<<fetch.cur_inst_tags_d[0];
        log_file<<" | Instruction_d: "<<setw(10)<<hex<<fetch.instruction_d[0]<<endl;
        log_file<<dec << "ALUSrc1: "<<fetch.alu_src1_d[0] <<" | ALU_src2: "<<fetch.alu_src2_d[0]<<endl;
        log_file << "Operand1: Valid: "<<dispatch.operand1_valid_out[0]<<" | Value: "<<dispatch.operand1_out[0]<<" | Tag: "<<dispatch.operand1_tag_out[0]<<endl;
        log_file << "Operand2: Valid: "<<dispatch.operand2_valid_out[0]<<" | Value: "<<dispatch.operand2_out[0]<<" | Tag: "<<dispatch.operand2_tag_out[0]<<endl;
        
        log_file<<"---------------"<<endl;
        log_file<<"ALU1: ";
        log_file <<"Issue Valid: "<<fetch.instruction_valid_d[1]<< " | Tag: "<<fetch.cur_inst_tags_d[1];
        log_file<<" | Instruction_d: "<<setw(10)<<hex<<fetch.instruction_d[1]<<endl;
        log_file << "ALUSrc1: "<<fetch.alu_src1_d[1] <<" | ALU_src2: "<<fetch.alu_src2_d[1]<<endl;
        log_file << "Operand1: Valid: "<<dispatch.operand1_valid_out[1]<<" | Value: "<<dispatch.operand1_out[1]<<" | Tag: "<<dispatch.operand1_tag_out[1]<<endl;
        log_file << "Operand2: Valid: "<<dispatch.operand2_valid_out[1]<<" | Value: "<<dispatch.operand2_out[1]<<" | Tag: "<<dispatch.operand2_tag_out[1]<<endl;
        log_file<<"---------------"<<endl;
        log_file<<"LSU: ";
        log_file <<"Issue Valid: "<<fetch.instruction_valid_d[2]<< " | Tag: "<<fetch.cur_inst_tags_d[2];
        log_file<<" | Instruction_d: "<<setw(10)<<hex<<fetch.instruction_d[2]<<endl;
        log_file <<dec<< "Operand1: Valid: "<<dispatch.operand1_valid_out[2]<<" | Value: "<<dispatch.operand1_out[2]<<" | Tag: "<<dispatch.operand1_tag_out[2]<<endl;
        log_file << "Operand2: Valid: "<<dispatch.operand2_valid_out[2]<<" | Value: "<<dispatch.operand2_out[2]<<" | Tag: "<<dispatch.operand2_tag_out[2]<<endl;
        
        log_file<<"---------------"<<endl;
        log_file<<"Branch: ";
        log_file <<"Issue Valid: "<<fetch.instruction_valid_d[3]<< " | Tag: "<<fetch.cur_inst_tags_d[3] << " | Type: "<<dispatch.branch_type_out;
        log_file<<" | Instruction_d: "<<setw(10)<<hex<<fetch.instruction_d[3]<<endl;
        log_file <<dec<< "Operand1: Valid: "<<dispatch.operand1_valid_out[3]<<" | Value: "<<dispatch.operand1_out[3]<<" | Tag: "<<dispatch.operand1_tag_out[3]<<endl;
        log_file << "Operand2: Valid: "<<dispatch.operand2_valid_out[3]<<" | Value: "<<dispatch.operand2_out[3]<<" | Tag: "<<dispatch.operand2_tag_out[3]<<endl;

        log_file<<endl;
    }

    void print_broadcast(){
        log_file << "- - - - -"<<endl;
        for(int i = 0; i < NUM_BROADCASTS; i++){
            log_file << "Broadcast " << i << ": Valid: " << broadcast.valids[i] << "| Tag: " << broadcast.tags[i] << "| Result: " << broadcast.results[i] << endl;
        }
        log_file << "Branch Broadcast - Valid: " << broadcast_branch_valid
        << "| Tag: " << broadcast_branch_tag
        << "| Result: "<<hex << broadcast_branch_result << endl;
    }

    void run(int num_cycles) {
        for (int i = 0; i < num_cycles; ++i) {
            log_file<<endl;
            log_file<<endl;
            log_file<<"==================================================================="<<endl;
            log_file << "Cycle " << i << endl;
            log_file<<"================================================================="<<endl;
            process_cycle();            
        }
    }
};

int main() {
    Processor proc;
    proc.run(10);  // Simulate 100 cycles
    log_file<<"Simulation complete."<<endl;
    for(int i = 0; i < 31; i++){
        log_file<<setw(2)<<i<<": "<<proc.dispatch.cur_register_file.reg_valid[i]<<" "<<setw(4)<<proc.dispatch.cur_register_file.reg_valid[i]<<" "<<setw(10)<< proc.dispatch.cur_register_file.reg_file[i]<<endl;
    }
    cout<<"Check The Execution Log For Run Details"<<endl;
    return 0;
}

