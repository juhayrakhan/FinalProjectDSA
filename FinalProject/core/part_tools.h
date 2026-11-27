// part_tools.h - (included inside struct Engine) invariant checker and seeded fuzzer (Section 5.12).
void finishCheck() {
    pl("invariants: all ok (checked after " + num(checkCount) + " commands)");
    checkOn = false; checkCount = 0;
}
void cmdCheck(const string& a) {
    if (a == "on") { checkOn = true; checkCount = 0; }
    else if (a == "off") finishCheck();
    else usage("check on|off");
}
bool fail(const string& name, const string& detail) {
    cout << "INVARIANT FAILED: " << name << ": " << detail << "\n";   // always visible, even when quiet
    halted = true; checkOn = false;
    return false;
}
bool cycRec(Doc* d, int gray, int black) {
    d->visit = gray;
    for (int i = 0; i < d->out[1].size(); i++) {
        Doc** t = docById.find(d->out[1][i].other);
        if (!t) continue;
        if ((*t)->visit == gray) return true;
        if ((*t)->visit != black && cycRec(*t, gray, black)) return true;
    }
    d->visit = black;
    return false;
}
bool runCheck() {
    Array<LineTree*> sv; Array<char> owned;    // saved text of every document
    for (int i = 0; i < docs.size(); i++) {
        Doc* d = docs[i];
        if (d->staged.size() == 0) { sv.push(&d->tree); owned.push(0); }
        else { LineTree* t = new LineTree; replay(d->head, *t); sv.push(t); owned.push(1); }
    }
    bool ok = checkInner(sv);
    for (int i = 0; i < sv.size(); i++) if (owned[i]) delete sv[i];
    return ok;
}
bool checkInner(Array<LineTree*>& sv) {
    string why;
    // AVL trees and LineID maps
    for (int i = 0; i < docs.size(); i++) {
        if (!docs[i]->tree.verify(why)) {
            int k = (int)why.find('|');
            return fail(why.substr(0, k), docs[i]->node->name + ", " + why.substr(k + 1));
        }
    }
    // commit DAG
    for (int i = 0; i < commits.nb; i++)
        for (HashMap<unsigned long long, Commit*, HId>::Node* x = commits.b[i]; x; x = x->next) x->val->chk = 0;
    for (int i = 0; i < commits.nb; i++)
        for (HashMap<unsigned long long, Commit*, HId>::Node* x = commits.b[i]; x; x = x->next) {
            Commit* c = x->val;
            int want = c->p1 ? 1 + imax(c->p1->depth, c->p2 ? c->p2->depth : 0) : 0;
            if (c->depth != want) return fail("dag-depth", cid(c->num) + " has depth " + num(c->depth) + ", expected " + num(want));
            if (c->p1) c->p1->chk++;
            if (c->p2) c->p2->chk++;
        }
    for (int i = 0; i < docs.size(); i++) {
        Commit** h = commits.find(docs[i]->head->num);
        if (!h || *h != docs[i]->head) return fail("dag-head", docs[i]->node->name + " head does not exist");
        docs[i]->head->chk++;
    }
    for (int i = 0; i < commits.nb; i++)
        for (HashMap<unsigned long long, Commit*, HId>::Node* x = commits.b[i]; x; x = x->next)
            if (x->val->refs != x->val->chk)
                return fail("dag-refcount", cid(x->val->num) + " has count " + num(x->val->refs) + ", expected " + num(x->val->chk));
    // B+ tree structure
    if (!index.verify(why)) return fail("bplus-structure", why);
    // index == tokens of saved text (both directions)
    for (int i = 0; i < docs.size(); i++) {
        Doc* d = docs[i];
        Array<string> tk;
        for (LNode* x = sv[i]->first(); x; x = LineTree::next(x)) {
            distinctTok(x->text, tk);
            for (int j = 0; j < tk.size(); j++) {
                const Array<Posting>* L = index.get(tk[j]);
                bool f = false;
                if (L) for (int q = 0; q < L->size(); q++) if ((*L)[q].file == d->id && (*L)[q].line == x->id) f = true;
                if (!f) return fail("index-missing", d->node->name + " " + lid(x->id) + " lacks a posting for '" + tk[j] + "'");
            }
        }
    }
    for (BNode* l = index.firstLeaf(); l; l = l->next)
        for (int i = 0; i < l->nk; i++)
            for (int j = 0; j < l->post[i]->size(); j++) {
                Posting& p = (*l->post[i])[j];
                int di = -1;
                for (int q = 0; q < docs.size(); q++) if (docs[q]->id == p.file) di = q;
                if (di < 0) return fail("index-doc", "posting of '" + l->keys[i] + "' names a deleted document F" + num(p.file));
                LNode* n = sv[di]->find(p.line);
                if (!n) return fail("index-line", "posting of '" + l->keys[i] + "' names " + lid(p.line) + " which is not in " + docs[di]->node->name);
                Array<string> tk; distinctTok(n->text, tk);
                if (!hasTok(tk, l->keys[i])) return fail("index-token", lid(p.line) + " does not contain '" + l->keys[i] + "'");
            }
    // Bloom filters: no false negatives
    for (int i = 0; i < docs.size(); i++) {
        if (docs[i]->inBin) continue;
        Array<string> tk;
        for (LNode* x = sv[i]->first(); x; x = LineTree::next(x)) {
            tokenize(x->text, tk);
            for (int j = 0; j < tk.size(); j++)
                if (!docs[i]->head->bloom->test(tk[j])) return fail("bloom-false-negative", docs[i]->node->name + " head filter misses '" + tk[j] + "'");
        }
    }
    // link and embed graphs == graphs rebuilt from the saved text
    for (int i = 0; i < docs.size(); i++) {
        Doc* d = docs[i];
        for (int k = 0; k < 2; k++) {
            if (d->inBin) {
                if (d->out[k].size() || d->in[k].size() || d->unres[k].size()) return fail("graph-bin", d->node->name + " is in the bin but has graph entries");
                continue;
            }
            Array<Delta> ex;
            for (LNode* x = sv[i]->first(); x; x = LineTree::next(x)) textDeltas(d, x->text, 1, ex);
            int entries = 0;
            for (int j = 0; j < ex.size(); j++) {
                if (ex[j].k != k) continue;
                entries++;
                Doc* t = docAt(ex[j].path);
                bool good = false;
                if (t) {
                    Edge* e = findEdge(d->out[k], t->id);
                    if (e && e->count == ex[j].d) good = true;
                    else if (k == 1 && unresCount(d, k, ex[j].path) == ex[j].d) good = true;   // cycle-skipped embed
                } else good = unresCount(d, k, ex[j].path) == ex[j].d;
                if (!good) return fail(k ? "embed-graph" : "link-graph", d->node->name + " -> " + ex[j].path + " differs from the rebuilt graph");
            }
            if (d->out[k].size() + d->unres[k].size() != entries) return fail(k ? "embed-graph" : "link-graph", d->node->name + " has entries that the saved text does not explain");
            for (int j = 0; j < d->out[k].size(); j++) {
                Doc** t = docById.find(d->out[k][j].other);
                Edge* r = t ? findEdge((*t)->in[k], d->id) : 0;
                if (!t || (*t)->inBin || !r || r->count != d->out[k][j].count) return fail(k ? "embed-graph" : "link-graph", d->node->name + " outgoing edge has no matching backlink");
            }
            for (int j = 0; j < d->in[k].size(); j++) {
                Doc** s = docById.find(d->in[k][j].other);
                Edge* r = s ? findEdge((*s)->out[k], d->id) : 0;
                if (!s || !r || r->count != d->in[k][j].count) return fail(k ? "embed-graph" : "link-graph", d->node->name + " backlink has no matching edge");
            }
        }
    }
    int gray = visitCtr + 1, black = visitCtr + 2;
    visitCtr += 2;
    for (int i = 0; i < docs.size(); i++)
        if (!docs[i]->inBin && docs[i]->visit != black && cycRec(docs[i], gray, black)) return fail("embed-cycle", "the embed graph has a cycle");
    // blocklist
    for (unsigned int i = 0; i < bl.m; i++) if (bl.c[i] > 15) return fail("blocklist-counter", "a counter is above 15");
    for (int i = 0; i < blocked.nb; i++)
        for (HashMap<string, char, HStr>::Node* x = blocked.b[i]; x; x = x->next)
            if (!bl.allPositive(x->key)) return fail("blocklist-counter", "blocked word '" + x->key + "' has a zero counter");
    return true;
}

// ---------------------------------------------------------------- seeded fuzzer
struct FuzzIn : Input {                        // answers merge prompts and records them
    unsigned int* st; ofstream* f;
    bool line(string& o) override {
        o = (Engine::rnd(*st, 2) == 0) ? "keep" : "take";
        (*f) << o << "\n"; f->flush();
        return true;
    }
    bool isScript() override { return false; }
};
static int rnd(unsigned int& st, int n) {               // state = (state * 1103515245 + 12345) mod 2^31
    st = (unsigned int)(((unsigned long long)st * 1103515245ULL + 12345ULL) % 2147483648ULL);
    return (int)((st >> 8) % (unsigned int)n);
}
static string genLine(Engine& e, unsigned int& st) {
    static const char* W[] = {"eggs", "flour", "milk", "honey", "maple", "syrup", "pan", "salt", "sugar", "cake", "spam", "oven"};
    static const char* A[] = {"alice", "bob", "carol"};
    auto docn = [&]() { return "d" + num(1 + rnd(st, 6)) + ".txt"; };
    auto text = [&]() {
        string s; int nw = 1 + rnd(st, 5);
        for (int i = 0; i < nw; i++) { if (i) s += " "; s += W[rnd(st, 12)]; }
        int x = rnd(st, 14);
        if (x == 0) s += " [[" + docn() + "]]";
        if (x == 1) s += " {{embed:" + docn() + "}}";
        return "\"" + s + "\"";
    };
    int r = rnd(st, 100);
    bool needDoc = (r >= 23 && r < 80) || r == 94 || r == 97 || r == 98 || r == 99;
    if (needDoc && !e.cur) return "open " + docn();
    string au = A[rnd(st, 3)]; //he
    int sz = e.cur ? e.cur->tree.size() : 0;
    if (r < 4) return "mkdir f" + num(1 + rnd(st, 2));
    if (r < 8) { int x = rnd(st, 3); return x == 0 ? string("cd ..") : x == 1 ? string("cd /") : "cd f" + num(1 + rnd(st, 2)); }
    if (r < 16) return "new " + docn();
    if (r < 23) return "open " + docn();
    if (r < 49) return "as " + au + " add " + text();
    if (r < 56) return "as " + au + " insert " + num(1 + rnd(st, sz + 1)) + " " + text();
    if (r < 63) return sz ? "as " + au + " edit " + num(1 + rnd(st, sz)) + " " + text() : "as " + au + " add " + text();
    if (r < 69) return sz ? "as " + au + " delete " + num(1 + rnd(st, sz)) : "as " + au + " add " + text();
    if (r < 77) return "save";
    if (r < 80) return "discard";
    if (r < 84) return "fork " + docn() + " " + docn();
    if (r < 89) return "merge " + docn() + " into " + docn();
    if (r == 89) return (rnd(st, 2) ? "star " : "unstar ") + docn();
    if (r == 90) return rnd(st, 4) ? "rm " + docn() : "rm f" + num(1 + rnd(st, 2));
    if (r == 91) return "restore " + num(1 + rnd(st, 4));
    if (r == 92) return rnd(st, 3) == 0 ? string("emptybin") : string("bin");
    if (r == 93) return (rnd(st, 2) ? "block " : "unblock ") + string("spam");
    if (r == 94) return "as " + au + " embed " + docn();
    if (r == 95) { string w = W[rnd(st, 12)]; return rnd(st, 2) ? "search " + w : "search " + w.substr(0, 2) + "*"; }
    if (r == 96) return "similar 3 30";
    if (r == 97) return rnd(st, 2) ? string("backlinks") : string("related 2");
    if (r == 98) { int x = rnd(st, 4); return x == 0 ? string("broken") : x == 1 ? string("orphans") : x == 2 ? string("log") : string("view c1"); }
    int x = rnd(st, 4);
    return x == 0 ? string("show") : x == 1 ? string("authors") : x == 2 ? string("ls") : string("props " + docn());
}
void cmdFuzz(const string& ss, const string& cs) {
    int seed, count;
    if (!parseInt(ss, seed) || !parseInt(cs, count)) { usage("fuzz <seed> <count>"); return; }
    string fname = "fuzz_" + ss + ".txt";
    ofstream f(fname.c_str());
    if (!f) { err("cannot write " + fname); return; }
    unsigned int st = (unsigned int)seed;
    Engine fe(BP_FUZZ_ORDER, true);                        // fresh engine, B+ tree order 3, output muted
    FuzzIn fi; fi.st = &st; fi.f = &f;
    fe.in = &fi;
    f << "check on\n";
    fe.exec("check on");
    int ran = 0;
    for (int i = 0; i < count && !fe.halted; i++) {
        string line = genLine(fe, st);
        f << line << "\n"; f.flush();          // written as generated, so a crash can be replayed
        fe.exec(line);
        ran++;
    }
    pl("fuzz " + ss + ": " + num(ran) + " commands run, written to " + fname);
    if (!fe.halted) pl("invariants: all ok (checked after " + num(fe.checkCount) + " commands)");
}
