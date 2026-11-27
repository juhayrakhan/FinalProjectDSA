// main.cpp - PaperTrail entry point.
//   ./papertrail            interactive console
//   ./papertrail script.txt run a script, then behave as if `exit` was typed
// Build: g++ -std=c++17 -Wall -Wextra -o papertrail main.cpp
#include "tracker.h"      // must come first: replaces global operator new/delete
#include "core/engine.h"
using namespace std;

int main(int argc, char** argv) {
    trk::made = 0; trk::freed = 0;
    int rc = 0;
    {
        Engine e(BP_ORDER, false);             // compile-time B+ tree order (fuzz runs use BP_FUZZ_ORDER = 3)
        if (argc > 1) {
            FileInput f(argv[1]);
            if (!f.ok) { cout << "error: cannot open " << argv[1] << "\n"; rc = 1; }
            else { e.runLoop(f); if (e.checkOn) e.finishCheck(); }
        } else {
            StdinInput si;
            e.runLoop(si);
        }
        if (e.halted) rc = 2;
    }                                           // everything is freed here
    unsigned long live = trk::made - trk::freed;
    cout << "allocations: " << trk::made << " made, " << trk::freed << " freed, " << live << " live\n";
    if (live != 0) { cout << "LEAK: " << live << " live allocations\n"; return 1; }
    return rc;
}
