#include <bits/stdc++.h>
#include "Constants.h"
using namespace std;
using namespace Constants;

#ifndef BROADCAST_H
#define BROADCAST_H
class Broadcast {
    public: 
    vector<int> results;
    vector<int> tags;
    vector<bool> valids;
    Broadcast() {
        results = vector<int>(NUM_BROADCASTS);
        tags = vector<int>(NUM_BROADCASTS);
        valids = vector<bool>(NUM_BROADCASTS);
    }
};

#endif