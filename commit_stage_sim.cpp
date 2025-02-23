#include "commit_stage.cpp"
#include <bits/stdc++.h>
using namespace std;
int main() {
    cout << "=== CommitStage Testbench Simulation ===" << endl;
    
    // Create an instance of CommitStage.
    CommitStage commitStage;

    // --- Cycle 0: Reset the commit stage ---
    cout << "\nCycle 0: Reset" << endl;
    // Assert rst = true; flush = false; stall = false.
    commitStage.process_cycle(true, false, false);
    cout<<"DONE PROCESS"<<endl;
    commitStage.commit_cycle();
    cout<<"DONE COMMIT"<<endl;
    commitStage.update_internal_signals();
    cout<<"DONE UPDATE"<<endl;

    // Print out the commit_valid signals after reset.
    for (int i = 0; i < ISSUE_WIDTH; i++) {
        cout << "After reset, commit_valid[" << i << "] = " << commitStage.commit_valid[i] << endl;
    }
    
    // --- Cycle 1: Dispatch a set of instructions ---
    cout << "\nCycle 1: Dispatch instructions" << endl;
    // Prepare dispatch vectors: here we dispatch two instructions.
    vector<bool> dispatch_en = { true, true, false, false };
    // Instruction types: _normal_inst and _branch (the enum values defined in the code)
    vector<int> dispatch_type = { _normal_inst, _branch, _normal_inst, _store };
    // Example destination registers (or addresses)
    vector<int> dispatch_dest = { 10, 20, 30, 40 };
    
    // Load dispatch signals into the unit.
    commitStage.process_dispatch(dispatch_en, dispatch_type, dispatch_dest);
    // Process a cycle with no reset/flush/stall.
    commitStage.process_cycle(false, false, false);
    commitStage.commit_cycle();
    commitStage.update_internal_signals();

    // Print commit stage outputs after dispatch.
    for (int i = 0; i < ISSUE_WIDTH; i++) {
        cout << "Cycle 1, commit_valid[" << i << "] = " << commitStage.commit_valid[i]
             << ", commit_value: " << commitStage.commit_values[i]
             << ", commit_dest: " << commitStage.commit_destinations[i]
             << ", commit_tag: " << commitStage.commit_tags[i] << endl;
    }
    
    // --- Cycle 2: Simulate broadcast of a result ---
    cout << "\nCycle 2: Broadcast result for an instruction" << endl;
    // Create a dummy Broadcast object.
    // (Assumes that Broadcast has public members: valids, tags, results.)
    Broadcast broadcast;
    broadcast.valids = { true, false };  // Only one broadcast is valid.
    broadcast.tags   = { 1, 2 };           // Tag '0' (i.e. first ROB entry) is broadcast.
    broadcast.results = { 100, 200 };         // The result value (for testing).
    
    // Process the broadcast. Here broadcast_branch_valid is false.
    commitStage.process_broadcast(broadcast, false, 0, 0);
    // Process another cycle to update ROB entries based on the broadcast.
    commitStage.process_cycle(false, false, false);
    commitStage.commit_cycle();
    commitStage.update_internal_signals();
    
    // Print commit stage outputs after the broadcast.
    for (int i = 0; i < ISSUE_WIDTH; i++) {
        cout << "Cycle 2, commit_valid[" << i << "] = " << commitStage.commit_valid[i]
             << ", commit_value: " << commitStage.commit_values[i]
             << ", commit_dest: " << commitStage.commit_destinations[i] << endl;
    }
    
    // --- Cycle 3: Dispatch a branch instruction to simulate misprediction ---
    cout << "\nCycle 3: Dispatch branch instruction to simulate misprediction" << endl;
    // For a branch misprediction, the branch result should have different high bits.
    // For example, a branch result with bit 31 != bit 30.
    // Here we simulate that by dispatching a branch instruction whose eventual result
    // would trigger the misprediction logic.
    dispatch_en = { true, false, false, false };
    // Dispatch a branch instruction.
    dispatch_type = { _branch, _normal_inst, _normal_inst, _normal_inst };
    // Use an arbitrary destination value. In a real test, you may want to
    // craft the value so that ((value>>31) != (value>>30)) becomes true.
    // For illustration, we set a value later via a broadcast.
    dispatch_dest = { 50, 0, 0, 0 };
    
    commitStage.process_dispatch(dispatch_en, dispatch_type, dispatch_dest);
    commitStage.process_cycle(false, false, false);
    commitStage.commit_cycle();
    
    // Now, simulate a broadcast that sets the branch instruction’s value.
    // For testing, let’s set the result value such that bit 31 = 1 and bit 30 = 0.
    broadcast.valids = { true, false };
    broadcast.tags   = { 0, 0 };
    // Set result to a value with bit31 high and bit30 low. For example, 0x80000000.
    broadcast.results = { int(0x80000000), 0 };
    commitStage.process_broadcast(broadcast, false, 0, 0);
    
    commitStage.process_cycle(false, false, false);
    commitStage.commit_cycle();
    commitStage.update_internal_signals();
    
    cout << "Cycle 3, commit_branch_misprediction = " << commitStage.commit_branch_misprediction << endl;
    cout << "Cycle 3, commit_branch_jump_address = " << commitStage.commit_branch_jump_address << endl;
    cout << "Cycle 3, commit_branch_jump_address_plus_one = " << commitStage.commit_branch_jump_address_plus_one << endl;
    cout << "Cycle 3, commit_branch_jump_address_plus_two = " << commitStage.commit_branch_jump_address_plus_two << endl;
    cout << "Cycle 3, commit_branch_jump_address_plus_three = " << commitStage.commit_branch_jump_address_plus_three << endl;

    cout << "\nSimulation complete." << endl;
    return 0;
}