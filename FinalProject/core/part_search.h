// part_search.h - (included inside struct Engine) refs, graphs, index, search, similarity, blocklist.

// ---------------------------------------------------------------- [[link]] and {{embed:x}} scanning
static bool validRef(const string& t) {
    if (t.empty()) return false;
    if (t[0] != '/') return validName(t);
    int i = 1;
    if (i >= len(t)) return false;
    while (i <= len(t)) {
        int j = (int)t.find('/', i);
        if (j < 0) j = len(t);
        if (!validName(t.substr(i, j - i))) return false;
        i = j + 1;
    }
    return true;
}
string absRef(Doc* d, const string& t) {
    if (t[0] == '/') return t;
    string f = d->node->parent->path;
    return f == "/" ? "/" + t : f + "/" + t;
}
// kind 0: [[x]]   kind 1: {{embed:x}}; appends the targets exactly as written
static void scanRefs(const string& text, int kind, Array<string>& out) {
    string op = kind ? "{{embed:" : "[[", cl = kind ? "}}" : "]]";
    int i = 0;
    for (;;) {
        int a = (int)text.find(op, i);
        if (a < 0) break;
        int s = a + len(op);
        int e = (int)text.find(cl, s);
        if (e < 0) break;
        string t = text.substr(s, e - s);
        if (validRef(t)) { out.push(t); i = e + 2; } else i = a + 1;
    }
}

// ---------------------------------------------------------------- graph primitives (k: 0 link, 1 embed)
static Edge* findEdge(Array<Edge>& a, unsigned int other) {
    for (int i = 0; i < a.size(); i++) if (a[i].other == other) return &a[i];
    return 0;
}
static void dropEdge(Array<Edge>& a, unsigned int other) {
    for (int i = 0; i < a.size(); i++) if (a[i].other == other) { a.removeAt(i); return; }
}
static void bump(Array<Edge>& a, unsigned int other, int d) {
    Edge* e = findEdge(a, other);
    if (e) { e->count += d; if (e->count <= 0) dropEdge(a, other); }
    else if (d > 0) { Edge x; x.other = other; x.count = d; a.push(x); }
}
void edgeAdd(Doc* s, Doc* t, int k, int d) { bump(s->out[k], t->id, d); bump(t->in[k], s->id, d); }
void unresAdd(Doc* s, int k, const string& path, int d) {
    Array<Unres>& a = s->unres[k];
    for (int i = 0; i < a.size(); i++)
        if (a[i].path == path) { a[i].count += d; if (a[i].count <= 0) a.removeAt(i); return; }
    if (d > 0) { Unres u; u.path = path; u.count = d; a.push(u); }
}
static int unresCount(Doc* s, int k, const string& path) {
    for (int i = 0; i < s->unres[k].size(); i++) if (s->unres[k][i].path == path) return s->unres[k][i].count;
    return 0;
}

// would adding the embed edge u -> v create a cycle? iterative DFS from v looking for u
bool wouldCycle(Doc* u, Doc* v, string& cyc) {
    if (u == v) { cyc = dispName(u) + " -> " + dispName(v); return true; }
    int st = ++visitCtr;
    Stack<Doc*> s;
    v->visit = st; v->par = 0;
    s.push(v);
    bool found = false;
    while (!s.empty()) {
        Doc* x = s.pop();
        if (x == u) { found = true; break; }
        for (int i = x->out[1].size() - 1; i >= 0; i--) {
            Doc** y = docById.find(x->out[1][i].other);
            if (y && (*y)->visit != st) { (*y)->visit = st; (*y)->par = x; s.push(*y); }
        }
    }
    if (!found) return false;
    Array<Doc*> path;
    for (Doc* x = u; x; x = x->par) path.push(x);            // u ... v
    cyc = "";
    for (int i = path.size() - 1; i >= 0; i--) cyc += dispName(path[i]) + " -> ";
    cyc += dispName(v);
    return true;
}
// adds d (+/-) references from s to path. mode 0: no cycle check, 1: check and fail, 2: check, else leave unresolved.
// *where reports what changed (1 = edge, 2 = unresolved list) so a rollback can undo it exactly.
bool addRef(Doc* s, int k, const string& path, int d, string* cyc, int mode, int* where = 0) {
    Doc* t = docAt(path);
    int w = 0;
    if (d > 0) {
        if (t) {
            if (k == 1 && mode != 0 && !findEdge(s->out[k], t->id)) {
                string c;
                if (wouldCycle(s, t, c)) {
                    if (mode == 1) { if (cyc) *cyc = c; return false; }
                    unresAdd(s, k, path, d);
                    if (where) *where = 2;
                    return true;
                }
            }
            edgeAdd(s, t, k, d); w = 1;
        } else { unresAdd(s, k, path, d); w = 2; }
    } else {
        if (t && findEdge(s->out[k], t->id)) { edgeAdd(s, t, k, d); w = 1; }
        else { unresAdd(s, k, path, d); w = 2; }
    }
    if (where) *where = w;
    return true;
}
static void addDelta(Array<Delta>& a, int k, const string& path, int d) {
    for (int i = 0; i < a.size(); i++) if (a[i].k == k && a[i].path == path) { a[i].d += d; return; }
    Delta x; x.k = k; x.path = path; x.d = d;
    a.push(x);
}
void textDeltas(Doc* doc, const string& text, int sign, Array<Delta>& out) {
    for (int k = 0; k < 2; k++) {
        Array<string> r;
        scanRefs(text, k, r);
        for (int i = 0; i < r.size(); i++) addDelta(out, k, absRef(doc, r[i]), sign);
    }
}
// updates both graphs from saved ops; an embed cycle rejects everything and rolls back
bool applyGraphOps(Doc* d, Ring<Op>& ops, string& cyc) {
    Array<Delta> ds;
    for (int i = 0; i < ops.size(); i++) {
        Op& o = ops.get(i);
        if (o.type == OP_INS) textDeltas(d, o.newText, 1, ds);
        else if (o.type == OP_DEL) textDeltas(d, o.oldText, -1, ds);
        else { textDeltas(d, o.oldText, -1, ds); textDeltas(d, o.newText, 1, ds); }
    }
    Array<int> where;                          // where each delta landed, for an exact rollback
    for (int i = 0; i < ds.size(); i++) {
        int w = 0;
        if (ds[i].d != 0 && !addRef(d, ds[i].k, ds[i].path, ds[i].d, &cyc, 1, &w)) {
            for (int j = i - 1; j >= 0; j--) {
                if (ds[j].d == 0) continue;
                if (where[j] == 1) { Doc* t = docAt(ds[j].path); if (t) edgeAdd(d, t, ds[j].k, -ds[j].d); }
                else unresAdd(d, ds[j].k, ds[j].path, -ds[j].d);
            }
            return false;
        }
        where.push(w);
    }
    return true;
}
void detachGraph(Doc* d) {                     // document goes to the bin
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < d->in[k].size(); i++) {
            Doc** s = docById.find(d->in[k][i].other);
            if (!s) continue;
            dropEdge((*s)->out[k], d->id);
            unresAdd(*s, k, d->node->path, d->in[k][i].count);   // the link becomes unresolved
        }
        d->in[k].clear();
        for (int i = 0; i < d->out[k].size(); i++) {
            Doc** t = docById.find(d->out[k][i].other);
            if (t) dropEdge((*t)->in[k], d->id);
        }
        d->out[k].clear();
        d->unres[k].clear();
    }
}
void rebuildGraph(Doc* d) {                    // rescans saved text (= working text when nothing is staged)
    for (LNode* x = d->tree.first(); x; x = LineTree::next(x))
        for (int k = 0; k < 2; k++) {
            Array<string> r;
            scanRefs(x->text, k, r);
            for (int i = 0; i < r.size(); i++) addRef(d, k, absRef(d, r[i]), 1, 0, k ? 2 : 0);
        }
}
// a document was created, forked or restored: matching unresolved entries become edges
void healAll() {
    for (int di = 0; di < docs.size(); di++) {
        Doc* s = docs[di];
        if (s->inBin) continue;
        for (int k = 0; k < 2; k++) {
            for (int i = 0; i < s->unres[k].size();) {
                Doc* t = docAt(s->unres[k][i].path);
                string c;
                if (t && !(k == 1 && wouldCycle(s, t, c))) {
                    edgeAdd(s, t, k, s->unres[k][i].count);
                    s->unres[k].removeAt(i);
                } else i++;
            }
        }
    }
}

// ---------------------------------------------------------------- index maintenance (saved text only)
void idxAdd(Doc* d, unsigned long long line, const string& text) {
    Array<string> tk; distinctTok(text, tk);
    Posting p; p.file = d->id; p.line = line;
    for (int i = 0; i < tk.size(); i++) index.insert(tk[i], p);
}
void idxDel(Doc* d, unsigned long long line, const string& text) {
    Array<string> tk; distinctTok(text, tk);
    Posting p; p.file = d->id; p.line = line;
    for (int i = 0; i < tk.size(); i++) index.remove(tk[i], p);
}
void indexOps(Doc* d, Ring<Op>& ops) {
    for (int i = 0; i < ops.size(); i++) {
        Op& o = ops.get(i);
        if (o.type == OP_INS) idxAdd(d, o.id, o.newText);
        else if (o.type == OP_DEL) idxDel(d, o.id, o.oldText);
        else {
            Array<string> a, b;
            distinctTok(o.oldText, a); distinctTok(o.newText, b);
            Posting p; p.file = d->id; p.line = o.id;
            for (int j = 0; j < a.size(); j++) if (!hasTok(b, a[j])) index.remove(a[j], p);
            for (int j = 0; j < b.size(); j++) if (!hasTok(a, b[j])) index.insert(b[j], p);
        }
    }
}

// ---------------------------------------------------------------- search
void cmdSearch(const string& arg) {
    string w = arg; bool prefix = false;
    if (!w.empty() && w[w.size() - 1] == '*') { prefix = true; w.erase(w.size() - 1); }
    bool good = !w.empty();
    for (int i = 0; i < len(w); i++) if (!isAN(w[i])) good = false;
    if (!good) { err("search takes one word or a prefix ending in *"); return; }
    w = lower(w);
    int live = 0, passed = 0;
    for (int i = 0; i < docs.size(); i++) {
        if (docs[i]->inBin) continue;
        live++;
        if (!prefix && docs[i]->head->bloom->test(w)) passed++;
    }
    Array<Posting> ps;
    if (prefix) {
        index.scanPrefix(w, ps);
        stableSort(ps, [](const Posting& a, const Posting& b) { return cmpPost(a, b) < 0; });
    } else {
        if (passed == 0) { pl("no matches"); return; }
        const Array<Posting>* L = index.get(w);
        if (L) for (int i = 0; i < L->size(); i++) ps.push((*L)[i]);
    }
    int matches = 0, ndocs = 0; unsigned int lastDoc = 0;
    Array<string> out;
    for (int i = 0; i < ps.size(); i++) {
        if (i && cmpPost(ps[i], ps[i - 1]) == 0) continue;           // duplicates (prefix queries)
        Doc** dp = docById.find(ps[i].file);
        if (!dp || (*dp)->inBin) continue;
        Doc* d = *dp;
        if (!prefix && !d->head->bloom->test(w)) continue;
        LNode* n = d->tree.find(ps[i].line);
        if (!n) continue;                                            // e.g. a staged deletion
        out.push(dispName(d) + ":" + num(d->tree.rank(n)) + " [" + n->author + "] " + n->text);
        matches++;
        if (d->id != lastDoc || ndocs == 0) { ndocs++; lastDoc = d->id; }
    }
    if (matches == 0) { pl("no matches"); return; }
    for (int i = 0; i < out.size(); i++) pl(out[i]);
    pl(num(matches) + (matches == 1 ? " match" : " matches") +
       " in " + plural(ndocs, "document") + " (bloom: " +
       (prefix ? string("skipped for prefix queries") : num(passed) + " of " + num(live) + " documents passed") + ")");
}

// ---------------------------------------------------------------- link graph queries
void cmdBacklinks() {
    if (!cur) { err("no document is open"); return; }
    for (int i = 0; i < cur->in[0].size(); i++) {
        Doc** s = docById.find(cur->in[0][i].other);
        if (s) pl(dispName(*s));
    }
}
void cmdRelated(const string& a) {
    int h;
    if (!parseInt(a, h) || h < 1) { usage("related <h>"); return; }
    if (!cur) { err("no document is open"); return; }
    int st = ++visitCtr;
    Ring<Doc*> q; Ring<int> dist;
    cur->visit = st;
    q.push(cur); dist.push(0);
    while (q.size()) {                         // breadth-first search along outgoing links
        Doc* x = q.pop(); int dx = dist.pop();
        if (dx >= h) continue;
        for (int i = 0; i < x->out[0].size(); i++) {
            Doc** y = docById.find(x->out[0][i].other);
            if (!y || (*y)->visit == st) continue;
            (*y)->visit = st;
            pl(num(dx + 1) + " " + dispName(*y));
            q.push(*y); dist.push(dx + 1);
        }
    }
}
void cmdBroken() {
    for (int i = 0; i < docs.size(); i++) {
        Doc* d = docs[i];
        if (d->inBin) continue;
        for (int k = 0; k < 2; k++)
            for (int j = 0; j < d->unres[k].size(); j++)
                pl(dispName(d) + " -> " + dispPath(d->unres[k][j].path) + (k ? " (embed)" : " (link)"));
    }
}
void cmdOrphans() {
    for (int i = 0; i < docs.size(); i++)
        if (!docs[i]->inBin && docs[i]->in[0].empty()) pl(dispName(docs[i]));
}

// ---------------------------------------------------------------- similarity checker (Section 5.10)
struct Sh { Doc* d; int n; Bloom* bf; HashMap<string, char, HStr>* set; Array<string>* list; };
void cmdSimilar(Array<string>& t) {
    int k, pct = 80;
    if (!parseInt(t[1], k) || k < 1 || (t.size() > 2 && (!parseInt(t[2], pct) || pct > 100))) { usage("similar <k> [percent]"); return; }
    Array<Sh> sh;
    for (int i = 0; i < docs.size(); i++) {
        Doc* d = docs[i];
        if (d->inBin) continue;
        Array<string> all, tk;
        for (LNode* x = d->tree.first(); x; x = LineTree::next(x)) {
            tokenize(x->text, tk);
            for (int j = 0; j < tk.size(); j++) all.push(tk[j]);
        }
        if (all.size() < 3) continue;
        Sh s; s.d = d; s.set = new HashMap<string, char, HStr>; s.list = new Array<string>;
        for (int j = 0; j + 2 < all.size(); j++) {
            string g = all[j] + " " + all[j + 1] + " " + all[j + 2];
            if (!s.set->find(g)) { s.set->put(g, 1); s.list->push(g); }
        }
        s.n = s.list->size();
        s.bf = new Bloom((unsigned int)(10 * s.n), 7);
        for (int j = 0; j < s.n; j++) s.bf->add((*s.list)[j]);
        sh.push(s);
    }
    MinHeap heap(k);
    int visit = 0;
    for (int i = 0; i < sh.size(); i++)
        for (int j = i + 1; j < sh.size(); j++) {
            visit++;
            long ab, ba, eab, eba;
            pairCounts(sh[i], sh[j], ab, ba, eab, eba);
            double est = pickFrac(ab, sh[i].n, ba, sh[j].n);
            if (est < pct / 100.0) continue;
            HeapItem it; it.score = est; it.visit = visit; it.a = i; it.b = j;
            heap.offer(it);
        }
    if (heap.h.size() == 0) pl("no pairs at or above " + num(pct) + "%");
    else {
        Array<HeapItem> res;
        while (heap.h.size()) res.push(heap.popMin());
        pl("rank A B bloom exact note");
        for (int r = res.size() - 1, rank = 1; r >= 0; r--, rank++) {
            Sh& A = sh[res[r].a]; Sh& B = sh[res[r].b];
            long ab, ba, eab, eba;
            pairCounts(A, B, ab, ba, eab, eba);
            long bn, bd, en, ed;
            pickPair(ab, A.n, ba, B.n, bn, bd);
            pickPair(eab, A.n, eba, B.n, en, ed);
            string line = num(rank) + " " + dispName(A.d) + " " + dispName(B.d) + " " + num(pctRound(bn, bd)) + "% " + num(pctRound(en, ed)) + "%";
            Array<Commit*> la, lb; Commit* lca; int sa, sb;
            lcaOf(A.d, B.d, lca, la, lb, sa, sb);
            if (lca) line += " fork-related (LCA " + cid(lca->num) + ")";
            pl(line);
        }
    }
    for (int i = 0; i < sh.size(); i++) { delete sh[i].bf; delete sh[i].set; delete sh[i].list; }
}
// shingles of A present in B (bloom / exact) and vice versa
void pairCounts(Sh& A, Sh& B, long& ab, long& ba, long& eab, long& eba) {
    ab = ba = eab = eba = 0;
    for (int i = 0; i < A.n; i++) { if (B.bf->test((*A.list)[i])) ab++; if (B.set->find((*A.list)[i])) eab++; }
    for (int i = 0; i < B.n; i++) { if (A.bf->test((*B.list)[i])) ba++; if (A.set->find((*B.list)[i])) eba++; }
}
static void pickPair(long x1, long d1, long x2, long d2, long& n, long& d) {   // the larger fraction
    if (x1 * d2 >= x2 * d1) { n = x1; d = d1; } else { n = x2; d = d2; }
}
static double pickFrac(long x1, long d1, long x2, long d2) {
    long n, d;
    pickPair(x1, d1, x2, d2, n, d);
    return (double)n / (double)d;
}

// ---------------------------------------------------------------- blocklist (Section 5.11)
bool blockedWordOk(const string& w) {
    if (w.empty()) return false;
    for (int i = 0; i < len(w); i++) if (!isAN(w[i])) return false;
    return true;
}
bool isBlockedTok(const string& tok) {            // counting filter in front of the exact set
    if (!bl.allPositive(tok)) { avoided++; return false; }
    return blocked.find(tok) != 0;
}
void warnText(int n, const string& text) {
    Array<string> tk, warned;
    tokenize(text, tk);
    for (int i = 0; i < tk.size(); i++)
        if (isBlockedTok(tk[i]) && !hasTok(warned, tk[i])) {
            warned.push(tk[i]);
            pl("warning: line " + num(n) + " contains blocked word \"" + tk[i] + "\"");
        }
}
void cmdBlock(const string& a) {
    string w = lower(a);
    if (!blockedWordOk(w)) { err("a blocked word may contain only letters and digits"); return; }
    if (blocked.find(w)) { err(w + " is already blocked"); return; }
    blocked.put(w, 1);
    bl.inc(w);
    pl("blocked " + w);
}
void cmdUnblock(const string& a) {
    string w = lower(a);
    if (!blocked.find(w)) { err(w + " is not blocked"); return; }
    blocked.erase(w);
    bl.dec(w);
    pl("unblocked " + w);
}
void cmdBlocked() {
    Array<string> ws;
    for (int i = 0; i < blocked.nb; i++)
        for (HashMap<string, char, HStr>::Node* x = blocked.b[i]; x; x = x->next) ws.push(x->key);
    stableSort(ws, [](const string& a, const string& b) { return a < b; });
    for (int i = 0; i < ws.size(); i++) pl(ws[i]);
    pl("exact lookups avoided: " + num(avoided));
}

// ---------------------------------------------------------------- false-positive experiment
void cmdFptest(const string& kind, const string& ms, const string& ks, const string& ns) {
    int m, k, n;
    bool counting = kind == "counting";
    if ((kind != "bloom" && !counting) || !parseInt(ms, m) || !parseInt(ks, k) || !parseInt(ns, n) ||
        m < 1 || k < 1 || n < 0 || m > 20000000 || k > 64) { usage("fptest <bloom|counting> <m> <k> <n>"); return; }
    int hits = 0, sat = 0;
    if (counting) {
        CountingBloom f((unsigned int)m, (unsigned int)k);
        for (int i = 0; i < n; i++) f.inc("w" + num(i));
        for (int i = 0; i < 10000; i++) if (f.allPositive("x" + num(i))) hits++;
        sat = f.saturated();
    } else {
        Bloom f((unsigned int)m, (unsigned int)k);
        for (int i = 0; i < n; i++) f.add("w" + num(i));
        for (int i = 0; i < 10000; i++) if (f.test("x" + num(i))) hits++;
    }
    double formula = pow(1.0 - exp(-(double)k * n / m), k) * 100.0;
    char b1[64], b2[64];
    snprintf(b1, sizeof b1, "%.2f%%", formula);
    snprintf(b2, sizeof b2, "%.2f%%", hits / 100.0);
    pl("fptest " + kind + " m=" + num(m) + " k=" + num(k) + " n=" + num(n));
    pl(string("formula : ") + b1 + " (1 - e^(-kn/m))^k");
    pl(string("measured : ") + b2 + " (" + num(hits) + " of 10000 queries answered \"probably present\")");
    if (counting) pl("counters saturated at 15: " + num(sat));
}
