// Load_Store_Functional_Unit.cpp
#include <bits/stdc++.h>
#include <cstdint>
#include "Broadcast.cpp"
#include "Constants.h"
using namespace std;
using namespace Constants;

//---------------------------------------------------------------------
// LSQ Issue structure (the “RS” entry for a load or a store)
struct LSQ_IssueData {
    int rob_tag;         // Tag coming from the ROB.
    bool base_reg_valid;      // Is the base reg value already valid?
    int base_reg_value;   // Base register (address) value.
    int base_reg_tag;    // Tag for the base register.
    bool store_value_valid;   // Is the store value valid?
    int store_value;      // For a store: value to store.
    int store_value_tag; // Tag for the store value.
    int16_t imm_value;       // Immediate offset (we assume 16‐bit immediate).
    bool type;                // Type: 1 → load, 0 → store.
};

//---------------------------------------------------------------------
// ==================== Load_Store_Queue ===============================
class Load_Store_Queue {
public:
    // Each LSQ entry holds the information of a load or a store.
    struct Entry {
        int rob_tag = 0;
        int base_reg_value = 0;
        int base_reg_tag = 0;
        bool base_reg_valid = 0;
        int store_value = 0;
        int store_value_tag = 0;
        bool store_value_valid = 0;
        int16_t imm_value = 0;
        bool type = 0;  // 1 for load, 0 for store.
        bool busy = 0;
    };

    struct Internal_Registers {
        int head;         // dequeue pointer
        int tail;         // enqueue pointer
        int count;        // number of entries in the queue
        Internal_Registers(){
            head= 0;
            tail = 0;
            count = 0;
        }
    };
    
    const int MAX_QUEUE_SIZE;
    // Public state (for simulation visibility)
    vector<Entry> current_entries;
    vector<Entry> next_entries;

    Internal_Registers cur_internal_reg;
    Internal_Registers next_internal_reg;

    bool valid_out = 0;
    bool full = 0;
    int tag_out = 0;
    int store_value_out = 0;
    int address_out = 0;
    bool type_out = 0;

    bool sb_full;
    bool sb_counter_overflow;

    Load_Store_Queue(int size)
      : MAX_QUEUE_SIZE(size)
    {
        current_entries.resize(MAX_QUEUE_SIZE);
        next_entries.resize(MAX_QUEUE_SIZE);

        cur_internal_reg = Internal_Registers();
        next_internal_reg = Internal_Registers();
    }

    // process_inputs mimics the LSQ always_ff block.
    // When flush or reset are true, the LSQ is cleared.
    // Otherwise, if issue_in is true (and the LSQ is not full) then the new
    // LSQ_IssueData is enqueued.
    // In addition, we check the broadcast bus to update any entries whose operands
    // are not yet valid.
    void process_inputs(bool issue_in, const LSQ_IssueData &issue_data,
                          const Broadcast &broadcast,
                          bool stall, bool flush, bool rst)
    {
        next_entries = current_entries;
        next_internal_reg = cur_internal_reg;

        auto& [head, tail, count] = next_internal_reg;
        if (flush || rst) {
            // Clear all entries.
            for (auto &entry : next_entries) {
                entry.busy = false;
                entry.base_reg_valid = false;
                entry.store_value_valid = false;
                entry.base_reg_tag = 0;
                entry.base_reg_valid = 0;
                entry.base_reg_value = 0;

                entry.store_value = 0;
                entry.store_value_valid = 0;
                entry.store_value_tag = 0;

                entry.rob_tag = 0;
                entry.imm_value = 0;
                entry.type = 0;
            }
            head = tail = count = 0;
            return;
        }
        
        // Enqueue a new instruction if possible.
        if (issue_in && !stall) {
            Entry &entry = next_entries[tail];
            entry.rob_tag         = issue_data.rob_tag;
            entry.base_reg_value  = issue_data.base_reg_value;
            entry.base_reg_tag    = issue_data.base_reg_tag;
            entry.base_reg_valid  = issue_data.base_reg_valid;
            entry.store_value     = issue_data.store_value;
            entry.store_value_tag = issue_data.store_value_tag;
            entry.store_value_valid = issue_data.store_value_valid;
            entry.imm_value       = issue_data.imm_value;
            entry.type            = issue_data.type;
            entry.busy            = true;
            tail = (tail + 1) % MAX_QUEUE_SIZE;
        }
        
        // Update the valid bits for both base register and store value by scanning the broadcast.
        for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
            if (next_entries[i].busy) {
                if (!next_entries[i].base_reg_valid) {
                    for (size_t j = 0; j < NUM_BROADCASTS; j++) {
                        if (next_entries[i].base_reg_tag == broadcast.tags[j] &&
                            broadcast.valids[j]) {
                            next_entries[i].base_reg_value = broadcast.results[j];
                            next_entries[i].base_reg_valid = true;
                        }
                    }
                }
                if (!next_entries[i].store_value_valid) {
                    for (size_t j = 0; j < NUM_BROADCASTS; j++) {
                        if (next_entries[i].store_value_tag == broadcast.tags[j] &&
                            broadcast.valids[j]) {
                            next_entries[i].store_value = broadcast.results[j];
                            next_entries[i].store_value_valid = true;
                        }
                    }
                }
            }
        }
        
        // Check if the head entry is “ready” to issue:
        // For a load, the only requirement is that the base register value is valid;
        // for a store, we also require the store value to be valid.
        if(valid_out){
            next_entries[head].busy = false;
            next_entries[head].busy = false;
            next_entries[head].base_reg_valid = false;
            next_entries[head].store_value_valid = false;
            next_entries[head].base_reg_tag = 0;
            next_entries[head].base_reg_valid = 0;
            next_entries[head].base_reg_value = 0;

            next_entries[head].store_value = 0;
            next_entries[head].store_value_valid = 0;
            next_entries[head].store_value_tag = 0;

            next_entries[head].rob_tag = 0;
            next_entries[head].imm_value = 0;
            next_entries[head].type = 0;
            head = (head + 1) % MAX_QUEUE_SIZE;
        }

        if(valid_out && !stall && issue_in) count = count;
        else if(issue_in && !stall) count++;
        else if(valid_out) count--;
    }

    //---------------------------------------------------------------------
    // Update the internal signals of the LSQ.
    // This includes valid_out, tag_out, address_out, store_value_out, type_out, and full.
    // The rules for determining valid_out are as follows:
    // - If the head entry is not busy, valid_out is 0.
    // - If the head entry is busy and the base register value is not valid, valid_out is 0.
    // - If the head entry is busy and the base register value is valid and the type is load, valid_out is 1.
    // - If the head entry is busy and the base register value is valid and the type is store and the store value is not valid, valid_out is 0.
    // - If the head entry is busy and the base register value is valid and the type is store and the store value is valid and the SB is not full and the age counter is not overflowing, valid_out is 1.
    void update_internal_signals(bool sb_full, bool sb_age_overflow){
        this->sb_full = sb_full;
        this->sb_counter_overflow = sb_age_overflow;

        Entry &headEntry = current_entries[ cur_internal_reg.head ];

        valid_out = headEntry.busy && headEntry.base_reg_valid &&
                    ((headEntry.type) ||
                    (!headEntry.type && headEntry.store_value_valid && !sb_full && !sb_counter_overflow));

        tag_out = headEntry.rob_tag;
        address_out = headEntry.base_reg_value + (headEntry.imm_value & ((1 << DATA_ADDR)-1));
        store_value_out = headEntry.store_value;
        type_out = headEntry.type;
        full = cur_internal_reg.count == MAX_QUEUE_SIZE;
    }

    void commit_cycle() { 
        // Move to next state - its relative position in the main code doesn't matter, what matter is that it comes after the process inputs method
        current_entries = next_entries; 
        cur_internal_reg = next_internal_reg;
    }
    bool is_full() { return cur_internal_reg.count == MAX_QUEUE_SIZE; }
};

//---------------------------------------------------------------------
// ==================== Store_Buffer ===================================
//
// The Store_Buffer holds “pending stores.” Each entry is tagged with an age.
// On commit the entry is marked as “ready” (ready to write to memory).
// In addition, the SB performs load–forwarding: when a load comes in, if there is a
// matching store (same address) the most recent (largest age) store is forwarded.
class Store_Buffer {
public:
    struct Entry {
        int tag = 0;
        int address = 0;
        int data = 0;
        int age = 1;
        bool busy = 0;
        bool ready = 0;
    };

    struct Internal_Registers {
        int age_counter = 1;
        bool flush_reg = 0;
    };
    
    
    vector<Entry> cur_entries;
    vector<Entry> next_entries;
    
    Internal_Registers cur_internal_reg;
    Internal_Registers next_internal_reg;
    
    bool age_counter_overflow = 0;
    const int AGE_MAX;    // maximum age (for masking/comparison)
    const int START_VALUE; // if age_counter < START_VALUE then we “stall”
    const int BUFFER_SIZE;

    bool load_match_found = 0;
    int load_value_out = 0;

    int wr_store_address = 0;
    bool wr_store_valid = 0;
    int wr_store_value = 0;

    int ready_slot;
    int lw_candidate;

    bool full = 0;
    bool age_counter_overflow_out = 0;

    // Status output: if the age counter is “overflowing” (i.e. we must stall LSQ)

    Store_Buffer(int buffer_size, int age_max, int start_value)
      : BUFFER_SIZE(buffer_size), AGE_MAX(age_max), START_VALUE(start_value)
    {
        cur_entries.resize(BUFFER_SIZE);
        next_entries.resize(BUFFER_SIZE);
    }

    // process_cycle implements the store buffer update.
    // flush or rst clears the SB.
    // When a new store is issued (issue_store_valid), it is inserted into a free slot.
    // The commit signals (commit_store_valids, commit_tags) mark matching entries as ready.
    // Finally, the logic finds a ready entry (by picking the one with the smallest age)
    // for memory write. Also, load forwarding is computed by scanning for any busy entry
    // whose address matches the load address.
    void process_inputs(bool flush, bool rst,
                       bool issue_store_valid, int issue_store_tag,
                       int issue_store_address, int issue_store_value,
                       const vector<bool>& commit_store_valids,
                       const vector<int>& commit_tags)
    {
        next_entries = cur_entries;
        next_internal_reg = cur_internal_reg;

        auto& age_counter = next_internal_reg.age_counter;
        auto& flush_reg = next_internal_reg.flush_reg;

        if (rst) {
            // Clear the SB (except perhaps the store being committed)
            for (auto &e : next_entries) {
                e.busy = false;
                e.ready = false;
                e.age = 0;
                e.tag = 0;
                e.address = 0;
                e.data = 0;
            }
            age_counter = START_VALUE;
        } else if(flush_reg) {
            for (auto &e : next_entries) {
                if(!(e.busy && e.ready)){
                    e.busy = false;
                    e.ready = false;
                    e.age = 0;
                    e.tag = 0;
                    e.address = 0;
                    e.data = 0;
                }
            }
            flush_reg = 0;
        }        
        else {
            flush_reg = flush;
            // Update age counter.
            if (age_counter_overflow)
                age_counter++;

            // Insert a new store if one is issued and the buffer is not full.
            if (issue_store_valid && full == 0) {
                int free_slot = find_free_slot();
                next_entries[free_slot].busy = true;
                next_entries[free_slot].tag = issue_store_tag;
                next_entries[free_slot].address = issue_store_address;
                next_entries[free_slot].data = issue_store_value;
                next_entries[free_slot].age = age_counter;
                age_counter = (age_counter == AGE_MAX) ? 0 : age_counter + 1;
            }

            get_ready_slot();
            // Mark entries as ready if a commit tag matches.
            if(next_entries[ready_slot].ready && wr_store_valid) {
                next_entries[ready_slot].busy = false;
                next_entries[ready_slot].ready = false;
                next_entries[ready_slot].age = 0;
                next_entries[ready_slot].tag = 0;
                next_entries[ready_slot].address = 0;
                next_entries[ready_slot].data = 0;
            }

            for (auto &e : next_entries) {
                for (size_t i = 0; i < commit_store_valids.size(); i++) {
                    if (commit_store_valids[i] && commit_tags[i] == e.tag && e.busy)
                        e.ready = true;
                }
            }
        }
    }

    void update_internal_signals(){
        if(cur_internal_reg.age_counter < START_VALUE) age_counter_overflow = 1;
        else age_counter_overflow = 0;
        full = is_full();

        get_ready_slot();
    }

    void commit_cycle(){
        cur_entries = next_entries;
        cur_internal_reg = next_internal_reg;
    }

    //---------------------------------------------------------------------
    // Update forwarding logic for loads
    // This method implements the load-forwarding logic for the Store Buffer.
    // It takes in the address of the load instruction and whether the load is valid.
    // The method checks each entry in the Store Buffer and computes a mask of their ages.
    // It then finds the entry with the highest age and if the load is valid and the age is not 0,
    // it sets load_match_found to 1 and load_value_out to the data value of that entry.
    // Otherwise, it sets load_match_found to 0 and load_value_out to 0.
    void update_forward_load(int issue_load_address, bool issue_load_valid){
        vector<int> masked_age(BUFFER_SIZE);
        for(int i = 0; i < BUFFER_SIZE; i++) 
            masked_age[i] = cur_entries[i].busy && (issue_load_address == cur_entries[i].address) ? cur_entries[i].age: 0;
        int max_age = 0;
        int candidate = 0;
        for(int i = 0; i < BUFFER_SIZE; i++) {
            if(masked_age[i] >= max_age && issue_load_address == cur_entries[i].address) {
                max_age = masked_age[i];
                candidate = i;
            }
        }
        if(issue_load_valid && cur_entries[candidate].busy){
            load_match_found = 1;
            load_value_out = cur_entries[candidate].data;
        } else {
            load_value_out = 0;
            load_match_found = 0;
        }
    }

    
private:
    void get_ready_slot(){
        vector<int> masked_age(BUFFER_SIZE);
        for(int i = 0; i < BUFFER_SIZE; i++) masked_age[i] = cur_entries[i].ready ? cur_entries[i].age : AGE_MAX;
        
        int min_age = AGE_MAX;
        int candidate = 0;
        
        for(int i = 0; i < BUFFER_SIZE; i++){
            if(masked_age[i] <= min_age) {
                min_age = masked_age[i];
                candidate = i;
            }
        }
        if(cur_entries[candidate].ready) {
            ready_slot = candidate;
            wr_store_valid = 1;
            wr_store_address = cur_entries[candidate].address;
            wr_store_value = cur_entries[candidate].data;
        }
        else {
            ready_slot = 0;
            wr_store_valid = 0;
            wr_store_address = 0;
            wr_store_value = 0;
        }
    }
    
    bool is_full() {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (!cur_entries[i].busy)
                return false;
        }
        return true;
    }
    int find_free_slot() {
        for (int i = BUFFER_SIZE - 1; i >= 0; i--) {
            if (!cur_entries[i].busy)
                return i;
        }
        return 0;
    }
};

//---------------------------------------------------------------------
// ==================== Data Memory (2–port) ==========================
//
// A very simple simulation of memory: we use a vector to represent memory.
// (In a more complete simulator you might have a detailed memory model.)
class DataMem2Port {
public:
    vector<int> mem;

    DataMem2Port() { 
        mem.resize((1 << DATA_ADDR), 0); 
        loadFromFile("data_memory.txt");
    }

    void read(int address, int& data) {
        data = mem[address];
    }

    void write(int address, int data) {
        mem[address] = data;
    }

private:
    void loadFromFile(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            throw runtime_error("Cannot open data memory file");
        }

        string line;
        int address = 0;
        while (getline(file, line) && address < mem.size()) {
            // Reads a line from the file, converts it to an unsigned long integer (with base 16)
            // and stores it in the memory at the current address. The address is then incremented.
            mem[address++] = stoul(line, nullptr, 16);
        }
    }
};

//---------------------------------------------------------------------
// ==================== Load_Store_Functional_Unit =======================
//
// This is the top–level simulation class for the load/store functional unit.
// It instantiates an LSQ, a Store_Buffer and a data memory.
// The process_inputs method “drives” the LSQ and SB modules
// (passing in the commit signals, broadcast, stall, flush, and reset).
// Get outputs -> Processr values -> Commit Cycle
class Load_Store_Functional_Unit {
public:
    Load_Store_Queue lsq;
    Store_Buffer sb;
    DataMem2Port mem;


    bool store_valid = 0;
    bool load_valid = 0;

    int valid_out = 0;   
    int tag_out = 0; 
    int data_out = 0;
    int address_out = 0; 
    int type_out = 0;    
    bool FU_full = 0;

    Load_Store_Functional_Unit(int lsq_size, int sb_size, int age_max, int start_value)
                            : lsq(lsq_size), sb(sb_size, age_max, start_value), mem()
    {
    }


    void process_inputs(bool issue_in, const LSQ_IssueData &lsq_issue_data,
                        const Broadcast &broadcast,
                        const vector<bool>& commit_store_valids,
                        const vector<int>& commit_tags,
                        bool stall, bool flush, bool rst)
    {
        // Get the values before updating
        data_out = lsq.valid_out;
        tag_out = lsq.tag_out;
        address_out = lsq.address_out;

        
        if(sb.load_match_found) {
            data_out = sb.load_value_out;
        }
        else {
            mem.read(lsq.address_out, data_out);
        }

        lsq.process_inputs(issue_in, lsq_issue_data, broadcast, stall, flush, rst);

        sb.process_inputs(flush, rst,
                         store_valid,lsq.tag_out, lsq.address_out, lsq.store_value_out,
                         commit_store_valids, commit_tags);


        sb.update_internal_signals();
        lsq.update_internal_signals(sb.full, sb.age_counter_overflow);

        sb.update_forward_load(lsq.address_out, lsq.type_out && lsq.valid_out);

        FU_full = lsq.is_full();
    }

    // commit_cycle would be called at the end of a cycle if you use
    // next-state buffering (as done in the ALU RS). 
    void commit_cycle() {
        // cur_internal_reg = next_internal_reg;
        lsq.commit_cycle();
        sb.commit_cycle();
    }
    bool is_full(){
        return lsq.is_full();
    }
};

