// part_history.h - (included inside struct Engine) commit DAG, log, view, fork, LCA, merge.

// ---------------------------------------------------------------- commits and reference counts
Commit* newCommit(Commit* p1, Commit* p2) {
    Commit* c = new Commit;
    c->num = ++nextCommit; c->p1 = p1; c->p2 = p2; c->tick = tick;
    c->depth = p1 ? 1 + getMax(p1->depth, p2 ? p2->depth : 0) : 0;
    c->bloom = new Bloom(bloomM, bloomK);
    if (p1) c->bloom->copyFrom(*p1->bloom);       // starts from a copy of the parent's filter
    if (p2) c->bloom->orWith(*p2->bloom);         // a merge ORs both parents' filters
    if (p1) p1->refs++;
    if (p2) p2->refs++;
    commits.put(c->num, c);
    return c;
}
// a commit is freed when its count (children + heads) reaches 0; parents' counts then drop, cascading
void release(Commit* c) {
    Stack<Commit*> s;
    s.push(c);
    while (!s.empty()) {
        Commit* x = s.pop();
        if (--x->refs > 0) continue;
        if (x->p1) s.push(x->p1);
        if (x->p2) s.push(x->p2);
        commits.erase(x->num);
        delete x;
    }
}
void setHead(Doc* d, Commit* c) {
    c->refs++;
    Commit* old = d->head;
    d->head = c;
    if (old) release(old);
}

// ---------------------------------------------------------------- stamping and LCA (Section 5.5)
// marks every ancestor of h (through all parents) with a fresh stamp; which=0 uses markA, 1 markB
int stamp(Commit* h, int which, DynamicArray<Commit*>* list) {
    int st = ++stampCtr;
    Stack<Commit*> s;
    s.push(h);
    while (!s.empty()) {
        Commit* c = s.pop();
        int& m = which ? c->markB : c->markA;
        if (m == st) continue;
        m = st;
        if (list) list->push(c);
        if (c->p1) s.push(c->p1);
        if (c->p2) s.push(c->p2);
    }
    return st;
}
// A's ancestors get markA == sa, B's get markB == sb; lca = deepest commit stamped by both
void lcaOf(Doc* a, Doc* b, Commit*& lca, DynamicArray<Commit*>& la, DynamicArray<Commit*>& lb, int& sa, int& sb) {
    sa = stamp(a->head, 0, &la);
    sb = stamp(b->head, 1, &lb);
    lca = 0;
    for (int i = 0; i < la.size(); i++) {
        Commit* c = la[i];
        if (c->markB != sb) continue;
        if (!lca || c->depth > lca->depth || (c->depth == lca->depth && c->num < lca->num)) lca = c;
    }
}
// text at commit `head`: walk first parents, stack them, pop oldest-first and apply
void replay(Commit* head, LineTree& t) {
    Stack<Commit*> s;
    for (Commit* c = head; c; c = c->p1) s.push(c);
    while (!s.empty()) {
        Commit* c = s.pop();
        for (int i = 0; i < c->ops.size(); i++) applyFwd(t, c->ops.get(i));
    }
}

void cmdLog() {
    if (!cur) { err("no document is open"); return; }
    for (Commit* c = cur->head; c; c = c->p1) {
        string s = cid(c->num) + " t=" + num(c->tick) + " depth " + num(c->depth);
        if (!c->p1) s += " root";
        else {
            s += " ops " + num(c->ops.size());
            if (c->p2) s += " merges " + cid(c->p2->num);
            DynamicArray<string> au;
            for (int i = 0; i < c->ops.size(); i++) if (!hasTok(au, c->ops.get(i).currentauthor)) au.push(c->ops.get(i).currentauthor);
            s += " authors ";
            for (int i = 0; i < au.size(); i++) s += (i ? ", " : "") + au[i];
        }
        pl(s);
    }
}
void cmdView(const string& a) {
    if (!cur) { err("no document is open"); return; }
    int k = 0;
    bool ok = len(a) > 1 && a[0] == 'c' && parseInt(a.substr(1), k);
    if (!ok) { usage("view <commit>"); return; }
    Commit* found = 0;
    for (Commit* c = cur->head; c; c = c->p1) if (c->num == k) found = c;
    if (!found) { err(a + " is not in the first-parent history of " + cur->node->name); return; }
    LineTree t;
    replay(found, t);
    pl("view " + a + " (read-only, " + plural(t.size(), "line") + ")");
    int n = 1;
    for (LNode* x = t.first(); x; x = LineTree::next(x), n++) pl(num(n) + " [" + x->author + "] " + x->text);
}

// ---------------------------------------------------------------- fork / common
void cmdFork(const string& src, const string& nn) {
    Doc* s = resolveDoc(src);
    if (!s) return;
    if (s->staged.size()) { err(s->node->name + " has staged changes; save or discard first"); return; }
    FNode* par = s->node->parent;
    if (!nameOk(nn, par)) return;
    if (docLimit()) return;
    FNode* node = mkNode(par, nn, true);
    Doc* d = newDocRaw(node);                  // new FileID, same folder
    d->tree.copyFrom(s->tree);                 // deep copy, same LineIDs
    d->head = s->head; s->head->refs++;        // shares the head commit
    for (int k = 0; k < 2; k++) {              // copies of the graph edges
        for (int i = 0; i < s->out[k].size(); i++) {
            Doc** t = docById.find(s->out[k][i].other);
            if (t) edgeAdd(d, *t, k, s->out[k][i].count);
        }
        for (int i = 0; i < s->unres[k].size(); i++) d->unres[k].push(s->unres[k][i]);
    }
    index.copyFile(s->id, d->id);              // copies of the index postings
    pl("created " + nn + " (F" + num(d->id) + ") as a fork of " + src + " at " + cid(d->head->num));
    healAll();
}
void cmdCommon(const string& a, const string& b) {
    Doc* x = resolveDoc(a); if (!x) return;
    Doc* y = resolveDoc(b); if (!y) return;
    DynamicArray<Commit*> la, lb; Commit* lca; int sa, sb;
    lcaOf(x, y, lca, la, lb, sa, sb);
    if (!lca) { err(a + " and " + b + " share no common ancestor"); return; }
    pl("common ancestor: " + cid(lca->num) + " (depth " + num(lca->depth) + ")");
    for (int side = 0; side < 2; side++) {
        DynamicArray<Commit*>& L = side ? lb : la;
        DynamicArray<int> ids;
        for (int i = 0; i < L.size(); i++) if ((side ? L[i]->markA != sa : L[i]->markB != sb)) ids.push(L[i]->num);
        stableSort(ids, [](int p1, int p2) { return p1 > p2; });
        string s = (side ? b : a) + " is " + plural(ids.size(), "commit") + " ahead";
        if (ids.size()) {
            s += " (";
            for (int i = 0; i < ids.size(); i++) s += (i ? ", " : "") + cid((unsigned int)ids[i]);
            s += ")";
        }
        pl(s);
    }
}

// ---------------------------------------------------------------- merge (Section 5.6)
// asks keep/take/abort; remembered answers per LineID are reused. 0 keep, 1 take, 2 abort
int resolveConflict(unsigned long long id, const string& kind, const string& curTxt, const string& incTxt, HashMap<unsigned long long, int, HId>& dec) {
    int* r = dec.find(id);
    if (r) return *r;
    pl("conflict on " + lid(id) + " (" + kind + ")");
    pl("current : " + curTxt);
    pl("incoming: " + incTxt);
    string prompt = "choose keep or take (or abort): ";
    for (;;) {
        p(prompt);
        string a;
        if (!readAnswer(a)) { pl(""); return 2; }       // script ended: the merge is aborted
        int v = -1;
        if (a == "keep") v = 0; else if (a == "take") v = 1; else if (a == "abort") v = 2;
        if (v < 0) { prompt = "choose keep, take or abort: "; continue; }
        if (v < 2) dec.put(id, v);
        return v;
    }
}
void cmdMerge(const string& inName, const string& tgName) {
    Doc* in2 = resolveDoc(inName); if (!in2) return;
    Doc* tg = resolveDoc(tgName); if (!tg) return;
    if (in2 == tg) { err("a document cannot be merged into itself"); return; }
    if (tg->staged.size()) { err(tg->node->name + " has staged changes; save or discard first"); return; }
    if (in2->staged.size()) { err(in2->node->name + " has staged changes; save or discard first"); return; }
    DynamicArray<Commit*> la, lb; Commit* lca; int sa, sb;
    lcaOf(tg, in2, lca, la, lb, sa, sb);          // markA = ancestors of target, markB = of incoming
    if (in2->head->markA == sa) { pl("already up to date"); return; }
    if (!lca) { err(inName + " and " + tgName + " share no common ancestor"); return; }

    // incoming ops: first-parent chain down to an ancestor of the target, replayed oldest-first
    Stack<Commit*> stk;
    for (Commit* c = in2->head; c && c->markA != sa; c = c->p1) stk.push(c);
    // target touched set: LineIDs edited/deleted on the target since the common history
    HashMap<unsigned long long, Touch, HId> touched;
    for (Commit* c = tg->head; c && c->markB != sb; c = c->p1) {
        for (int i = c->ops.size() - 1; i >= 0; i--) {
            Operation& o = c->ops.get(i);
            if (o.type == OP_INS || touched.find(o.id)) continue;
            Touch tc; tc.kind = o.type == OP_EDIT ? 1 : 2; tc.prev = o.anchor;
            touched.put(o.id, tc);
        }
    }
    LineTree work;                                 // working copy: abort simply frees it
    work.copyFrom(tg->tree);
    CircularQueue<Operation> applied;
    HashMap<unsigned long long, int, HId> dec;
    int nApp = 0, nSkip = 0, nKept = 0, nNote = 0;
    bool aborted = false;
    while (!stk.empty() && !aborted) {
        Commit* c = stk.pop();
        for (int i = 0; i < c->ops.size() && !aborted; i++) {
            Operation& o = c->ops.get(i);                 // iterated with get(i), never popped
            if (o.type == OP_INS) {
                if (work.find(o.id)) { nSkip++; continue; }
                unsigned long long anchorUsed = o.anchor; LNode* a = 0;
                if (o.anchor) {
                    a = work.find(o.anchor);
                    if (!a) {
                        a = work.last(); anchorUsed = a ? a->id : 0; nNote++;
                        pl("note: " + lid(o.id) + " appended at the end because its anchor " + lid(o.anchor) + " no longer exists");
                    }
                }
                LNode* nn = work.insertAfter(a, o.id, o.newText, o.currentauthor);
                Operation r; r.type = OP_INS; r.id = o.id; r.anchor = anchorUsed; r.newText = o.newText; r.currentauthor = o.currentauthor;
                applied.push(r); nApp++;
                warnText(work.rank(nn), o.newText);
            } else if (o.type == OP_DEL) {
                LNode* n = work.find(o.id);
                if (!n) { nSkip++; continue; }
                if (touched.find(o.id)) {
                    int ch = resolveConflict(o.id, "delete vs edit", "\"" + n->text + "\"", "(deleted)", dec);
                    if (ch == 2) { aborted = true; break; }
                    if (ch == 0) { nKept++; continue; }
                }
                int rk = work.rank(n);
                Operation r; r.type = OP_DEL; r.id = o.id; r.anchor = rk > 1 ? work.select(rk - 1)->id : 0;
                r.oldText = n->text; r.originalauthor = n->author; r.currentauthor = o.currentauthor;
                work.erase(n);
                applied.push(r); nApp++;
            } else {
                LNode* n = work.find(o.id);
                if (n && n->text == o.newText) { nSkip++; continue; }
                Touch* tc = touched.find(o.id);
                if (!n) {
                    if (!tc || tc->kind != 2) { nSkip++; continue; }
                    int ch = resolveConflict(o.id, "edit vs delete", "(deleted)", "\"" + o.newText + "\"", dec);
                    if (ch == 2) { aborted = true; break; }
                    if (ch == 0) { nKept++; continue; }
                    LNode* a = 0;                  // take: re-insert after the DeleteLine's prevLineID
                    if (tc->prev) { a = work.find(tc->prev); if (!a) a = work.last(); }
                    LNode* nn = work.insertAfter(a, o.id, o.newText, o.currentauthor);
                    Operation r; r.type = OP_INS; r.id = o.id; r.anchor = a ? a->id : 0; r.newText = o.newText; r.currentauthor = o.currentauthor;
                    applied.push(r); nApp++;
                    warnText(work.rank(nn), o.newText);
                    continue;
                }
                if (tc) {
                    int ch = resolveConflict(o.id, "edit vs edit", "\"" + n->text + "\"", "\"" + o.newText + "\"", dec);
                    if (ch == 2) { aborted = true; break; }
                    if (ch == 0) { nKept++; continue; }
                }
                Operation r; r.type = OP_EDIT; r.id = o.id; r.oldText = n->text; r.newText = o.newText;
                r.originalauthor = n->author; r.currentauthor = o.currentauthor;
                n->text = o.newText; n->author = o.currentauthor;
                applied.push(r); nApp++;
                warnText(work.rank(n), o.newText);
            }
        }
    }
    if (aborted) { pl("merge aborted, nothing changed"); return; }
    if (work.size() > 5000) { err("merge would exceed 5000 lines; nothing changed"); return; }
    string cyc;
    if (!applyGraphOps(tg, applied, cyc)) {
        err("merge rejected, it would create an embed cycle");
        pl("  " + cyc);
        return;
    }
    indexOps(tg, applied);
    Commit* c = newCommit(tg->head, in2->head);    // parent1 = old target head, parent2 = incoming head
    c->ops.takeFrom(applied);
    bloomOps(c->bloom, c->ops);
    setHead(tg, c);
    tg->tree.swapWith(work);
    tg->lastMod = tick;
    pl("merged " + inName + " into " + tgName + " as " + cid(c->num) + ": " + num(nApp) + " applied, " + num(nSkip) +
       " skipped, " + num(nKept) + " kept, " + num(nNote) + " noted");
}
