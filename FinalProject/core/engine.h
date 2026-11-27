// engine.h - the PaperTrail engine: model types and the Engine class.
// The Engine's member functions live in the part_*.h files, which are included
// INSIDE the class body (so every function sees every other one).
#pragma once
#include <iostream>
#include <fstream>
#include "../basic structures/dynamicarray.h"
#include "../basic structures/bloom.h"
#include "../basic structures/avl.h"
#include "../basic structures/arrayqueue.h"
#include "../basic structures/bplus.h"

using namespace std;

const int OP_INS = 0, OP_DEL = 1, OP_EDIT = 2;

// One operation. InsertLine: id=new line, anchor=line above, newText, author.
// DeleteLine: id, anchor=prevLineID, oldText, oldAuthor (author = who deleted, for the log).
// EditLine: id, oldText, newText, oldAuthor, author.
struct Op {
    int type; unsigned long long id, anchor;
    string oldText, newText, oldAuthor, author;
    Op() : type(0), id(0), anchor(0) {}
};

struct Commit {
    unsigned int num; Commit *p1, *p2;
    int tick, depth;
    CircularQueue<Op> ops;
    Bloom* bloom;
    int refs;                 // child commits + document heads
    int markA, markB, chk;    // traversal stamps / checker scratch
    Commit() : num(0), p1(0), p2(0), tick(0), depth(0), bloom(0), refs(0), markA(0), markB(0), chk(0) {}
    ~Commit() { delete bloom; }
};

struct Doc;
struct FNode {                // explorer (N-ary tree) node
    string name, path;
    bool file, starred;
    FNode* parent;
    DynamicArray<FNode*> kids;
    Doc* doc;
    FNode() : file(false), starred(false), parent(0), doc(0) {}
};
struct Edge { unsigned int other; int count; };
struct Unres { string path; int count; };

struct Doc {
    unsigned int id; FNode* node;
    LineTree tree;            // working text
    CircularQueue<Op> staged;
    Commit* head;
    int lastMod;
    bool inBin;
    DynamicArray<Edge> out[2], in[2];     // [0] links, [1] embeds
    DynamicArray<Unres> unres[2];
    int visit; Doc* par; bool expanding;
    Doc() : id(0), node(0), head(0), lastMod(0), inBin(false), visit(0), par(0), expanding(false) {}
};
struct BinItem { int num; FNode* node; string origParent; string origName; };
struct Delta { int k; string path; int d; };
struct Touch { int kind; unsigned long long prev; };        // kind 1 = edited, 2 = deleted on the target

struct Input {                // source of command lines (keyboard, script, fuzzer answers)
    virtual bool line(string& out) = 0;
    virtual bool isScript() = 0;
    virtual ~Input() {}
};
struct StdinInput : Input {
    bool line(string& out) override {
        if (!getline(cin, out)) return false;
        if (!out.empty() && out[out.size() - 1] == '\r') out.erase(out.size() - 1);
        return true;
    }
    bool isScript() override { return false; }
};
struct FileInput : Input {
    ifstream f; bool ok;
    explicit FileInput(const string& path) : f(path.c_str()) { ok = f.is_open(); }
    bool line(string& out) override {
        if (!getline(f, out)) return false;
        if (!out.empty() && out[out.size() - 1] == '\r') out.erase(out.size() - 1);
        return true;
    }
    bool isScript() override { return true; }
};

struct Engine {
    int bpOrder; bool quiet;
    int tick; unsigned long long nextLine; unsigned int nextFile, nextCommit;
    int binCtr, stampCtr, visitCtr;
    FNode* root; FNode* cwd; Doc* cur;
    HashMap<string, FNode*, HStr> paths;          // absolute path -> node (live nodes only)
    HashMap<unsigned long long, Doc*, HId> docById;           // FileID -> document
    HashMap<unsigned long long, Commit*, HId> commits;        // CommitID -> commit
    DynamicArray<Doc*> docs;                          // every document not yet permanently deleted, FileID order
    DynamicArray<BinItem*> bin;
    BPlus index;
    CountingBloom bl;
    HashMap<string, char, HStr> blocked;          // exact set behind the counting filter
    long avoided;
    Input* in; bool script, checkOn, halted, exiting;
    long checkCount; unsigned int bloomM, bloomK;

    Engine(int order, bool q)
        : bpOrder(order), quiet(q), tick(0), nextLine(0), nextFile(0), nextCommit(0),
          binCtr(0), stampCtr(0), visitCtr(0), root(0), cwd(0), cur(0),
          index(order), bl(1024, 3), avoided(0), in(0), script(false), checkOn(false),
          halted(false), exiting(false), checkCount(0), bloomM(32768), bloomK(3) {
        root = new FNode;
        root->path = "/";
        cwd = root;
        paths.put("/", root);
    }
    ~Engine() {
        for (int i = 0; i < docs.size(); i++) delete docs[i];
        for (int i = 0; i < commits.nb; i++)
            for (HashMap<unsigned long long, Commit*, HId>::Node* x = commits.b[i]; x; x = x->next) delete x->val;
        for (int i = 0; i < bin.size(); i++) { freeFN(bin[i]->node); delete bin[i]; }
        freeFN(root);
    }

#include "part_core.h"
#include "part_explorer.h"
#include "part_doc.h"
#include "part_history.h"
#include "part_search.h"
#include "part_tools.h"
};
