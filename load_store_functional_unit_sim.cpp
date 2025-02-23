// testbench.cpp
#include <iostream>
#include <vector>

// Include the simulation modules – make sure these paths match your project layout.
// #include "Broadcast.cpp"
#include "load_store_functional_unit.cpp"  // This should include LSQ, SB, DataMem2Port, etc.
#include "Constants.h"                  // Defines, for example, DATA_ADDR

using namespace std;
using namespace Constants;


// Main simulation testbench
int main() {
    // Create a Load_Store_Functional_Unit with:
    //   LSQ size = 4, Store Buffer size = 4, AGE_MAX = 16, START_VALUE = 1.
    Load_Store_Functional_Unit lsu(4, 4, 16, 1);

    // Simulation: run for a number of cycles.
    // For each cycle, we set up control signals, possibly issue an instruction,

    // provide commit signals, and then run one cycle.
    for (int cycle = 0; cycle < 8; cycle++) {
        bool flush = false, rst = false, stall = false;
        bool issue_in = false;
        LSQ_IssueData issue_data = {};  // default initialize all fields

        // Commit signals for the Store Buffer.
        vector<bool> commit_store_valids;
        vector<int> commit_tags;

        // For this testbench, we assume no operand broadcast (all operands already valid).
        Broadcast broadcast;

        cout << "--------------------- Cycle " << cycle << " ---------------------" << endl;

        // ----- Cycle-specific stimulus -----
        if (cycle == 0) {
            // Cycle 0: reset the unit.
            rst = true;
            cout << "[Cycle " << cycle << "] Reset asserted." << endl;
        }
        else if (cycle == 1) {
            // Cycle 1: Issue a STORE instruction.
            // Write 42 to address = base_reg_value (100) + imm_value (4) = 104.
            issue_in = true;
            issue_data.rob_tag         = 1;
            issue_data.base_reg_valid  = true;
            issue_data.base_reg_value  = 100;
            issue_data.base_reg_tag    = 0;   // Not used since valid already.
            issue_data.store_value_valid = true;
            issue_data.store_value     = 42;
            issue_data.store_value_tag = 0;   // Not used.
            issue_data.imm_value       = 4;
            issue_data.type            = false; // false indicates a STORE.
            cout << "[Cycle " << cycle << "] Issuing STORE (rob_tag=1) to write 42 at address 104." << endl;
        }
        else if (cycle == 2) {
            // Cycle 2: Commit the previously issued store.
            commit_store_valids= {false, true};
            commit_tags = {4, 1}; // Commit the store with rob_tag 1.
            cout << "[Cycle " << cycle << "] Committing STORE with rob_tag=1." << endl;
        }
        else if (cycle == 3) {
            // Cycle 3: Issue a LOAD instruction.
            // Load from the same address 104.
            issue_in = true;
            issue_data.rob_tag         = 2;
            issue_data.base_reg_valid  = true;
            issue_data.base_reg_value  = 100;
            issue_data.base_reg_tag    = 0;
            // For loads, the store value fields are ignored.
            issue_data.store_value_valid = false;
            issue_data.store_value     = 0;
            issue_data.store_value_tag = 0;
            issue_data.imm_value       = 4;
            issue_data.type            = true; // true indicates a LOAD.
            cout << "[Cycle " << cycle << "] Issuing LOAD (rob_tag=2) from address 104." << endl;
        }
        else if (cycle == 4) {
            // Cycle 4: Issue a second STORE instruction.
            // Write 84 to address = 200 + 8 = 208.
            issue_in = true;
            issue_data.rob_tag         = 3;
            issue_data.base_reg_valid  = true;
            issue_data.base_reg_value  = 200;
            issue_data.base_reg_tag    = 0;
            issue_data.store_value_valid = true;
            issue_data.store_value     = 84;
            issue_data.store_value_tag = 0;
            issue_data.imm_value       = 8;
            issue_data.type            = false; // STORE.
            cout << "[Cycle " << cycle << "] Issuing STORE (rob_tag=3) to write 84 at address 208." << endl;
        }
        else if (cycle == 5) {
            // Cycle 5: Issue a LOAD that should forward the pending store.
            // Load from address 208; since the store with rob_tag 3 is in the SB,
            // load–forwarding should return 84.
            issue_in = true;
            issue_data.rob_tag         = 4;
            issue_data.base_reg_valid  = true;
            issue_data.base_reg_value  = 200;
            issue_data.base_reg_tag    = 0;
            issue_data.store_value_valid = false;
            issue_data.store_value     = 0;
            issue_data.store_value_tag = 0;
            issue_data.imm_value       = 8;
            issue_data.type            = true; // LOAD.
            cout << "[Cycle " << cycle << "] Issuing LOAD (rob_tag=4) from address 208 (expecting forwarded value 84)." << endl;
        }
        else if (cycle == 6) {
            // Cycle 6: Commit the second store.
            commit_store_valids.push_back(true);
            commit_tags.push_back(3);
            cout << "[Cycle " << cycle << "] Committing STORE with rob_tag=3." << endl;
        }
        else {
            cout << "[Cycle " << cycle << "] No new instruction or commit." << endl;
        }

        // ----- Process one cycle -----
        // The order is important:
        // 1. Get the latest SB status.
        // 2. Process the LSQ (which uses the SB flags).
        // 3. Get LSQ outputs and then update the SB.
        lsu.process_next_cycle(issue_in, issue_data, broadcast,
            commit_store_valids, commit_tags,
            stall, flush, rst);
            
            
        cout<< "Processed inputs"<<endl;
        
        lsu.commit_cycle();
        lsu.update_internal_signals();
            
        // ----- Read final outputs -----
    
        // lsu.get_outputs(valid_out, tag_out, data_out, address_out, lsq_full);
        cout << "[Cycle " << cycle << "] LS Unit Outputs:" << endl;
        cout << "    valid_out = " << lsu.valid_out
             << ", rob_tag = " << lsu.tag_out
             << ", computed address = " << lsu.address_out
             << ", data (or store value) = " << lsu.data_out
             << ", LSQ_full = " << lsu.FU_full << endl;

        // ----- Check memory contents -----
        // After a commit the memory should be updated.
        int mem104, mem208;
        lsu.mem.read(104, mem104);
        lsu.mem.read(208, mem208);
        cout << "    Memory[104] = " << mem104
             << ", Memory[208] = " << mem208 << endl;
    }

    cout << "------------------ End of Simulation ------------------" << endl;
    return 0;
}
