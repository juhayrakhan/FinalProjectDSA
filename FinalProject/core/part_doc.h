// part_doc.h - (included inside struct Engine) documents, editing, staging, save, discard.

// ---------------------------------------------------------------- document lifecycle
Doc* newDocRaw(FNode* node) {
    Doc* d = new Doc;
    d->id = ++nextFile; d->node = node; d->lastMod = tick;
    node->doc = d;
    docs.push(d);
    docById.put(d->id, d);
    return d;
}
Doc* newDoc(FNode* node) {                     // every new document starts with a root commit
    Doc* d = newDocRaw(node);
    Commit* c = newCommit(0, 0);
    c->refs = 1;
    d->head = c;
    return d;
}
void killDoc(Doc* d) {                         // permanent deletion (emptybin)
    index.purgeFile(d->id);
    if (d->head) release(d->head);
    docById.erase(d->id);
    for (int i = 0; i < docs.size(); i++) if (docs[i] == d) { docs.removeAt(i); break; }
    delete d;
}

// ---------------------------------------------------------------- applying operations to a tree
static void applyFwd(LineTree& t, const Op& o) {
    if (o.type == OP_INS) {
        LNode* a = o.anchor ? t.find(o.anchor) : 0;
        if (o.anchor && !a) a = t.last();
        t.insertAfter(a, o.id, o.newText, o.author);
    } else if (o.type == OP_DEL) {
        LNode* n = t.find(o.id);
        if (n) t.erase(n);
    } else {
        LNode* n = t.find(o.id);
        if (n) { n->text = o.newText; n->author = o.author; }
    }
}
static void applyInv(LineTree& t, const Op& o) {
    if (o.type == OP_INS) {
        LNode* n = t.find(o.id);
        if (n) t.erase(n);
    } else if (o.type == OP_DEL) {
        LNode* a = o.anchor ? t.find(o.anchor) : 0;
        if (o.anchor && !a) a = t.last();
        t.insertAfter(a, o.id, o.oldText, o.oldAuthor);
    } else {
        LNode* n = t.find(o.id);
        if (n) { n->text = o.oldText; n->author = o.oldAuthor; }
    }
}

// ---------------------------------------------------------------- the "as <author> ..." commands
void cmdAs(Array<string>& t, const string& tail) {
    if (t.size() < 3) { usage("as <author> add|insert|edit|delete|embed ..."); return; }
    const string& sub = t[2];
    const string& author = t[1];
    bool hasText = sub == "add" || sub == "insert" || sub == "edit";
    int need = sub == "add" ? 3 : (sub == "insert" || sub == "edit" || sub == "delete" || sub == "embed") ? 4 : -1;
    if (need < 0) { usage("as <author> add|insert|edit|delete|embed ..."); return; }
    string us = sub == "add" ? "as <author> add \"text\"" : sub == "insert" ? "as <author> insert <n> \"text\"" :
             sub == "edit" ? "as <author> edit <n> \"text\"" : sub == "delete" ? "as <author> delete <n>" :
             "as <author> embed <name>";
    if (!hasText) { if (t.size() != need || !tail.empty()) { usage(us); return; } }
    else if (t.size() < need || (t.size() == need && tail.empty())) { usage(us); return; }
    if (!validAuthor(author)) { err("invalid author \"" + author + "\""); return; }
    if (!cur) { err("no document is open"); return; }
    Doc* d = cur;
    if (sub == "embed") { cmdEmbed(d, author, t[3]); return; }
    int n = 0;
    if (sub != "add" && !parseInt(t[3], n)) { usage(us); return; }
    string text;
    if (hasText) {
        if (t.size() > need) { err("text must be inside double quotes"); return; }
        if (!textArg(tail, text)) return;
    }
    int lines = d->tree.size();
    if (sub == "add" || sub == "insert") {
        int pos = sub == "add" ? lines + 1 : n;
        if (sub == "insert" && (n < 1 || n > lines + 1)) {
            err("cannot insert at line " + num(n) + " (the document has " + num(lines) + " lines; use 1 to " + num(lines + 1) + ")");
            return;
        }
        if (lines >= 5000) { err("limit of 5000 lines per document reached"); return; }
        doInsert(d, pos, text, author);
        return;
    }
    if (n < 1 || n > lines) { err("line " + num(n) + " does not exist (the document has " + num(lines) + " lines)"); return; }
    LNode* x = d->tree.select(n);
    if (sub == "edit") {
        Op o; o.type = OP_EDIT; o.id = x->id; o.oldText = x->text; o.newText = text; o.oldAuthor = x->author; o.author = author;
        x->text = text; x->author = author;
        d->staged.push(o);
        pl("staged EditLine(" + lid(o.id) + ")");
        warnText(n, text);
    } else {
        Op o; o.type = OP_DEL; o.id = x->id; o.anchor = n > 1 ? d->tree.select(n - 1)->id : 0;
        o.oldText = x->text; o.oldAuthor = x->author; o.author = author;
        d->tree.erase(x);
        d->staged.push(o);
        pl("staged DeleteLine(" + lid(o.id) + ")");
    }
}
void doInsert(Doc* d, int pos, const string& text, const string& author) {
    LNode* anchor = pos > 1 ? d->tree.select(pos - 1) : 0;
    unsigned long long id = ++nextLine;
    Op o; o.type = OP_INS; o.id = id; o.anchor = anchor ? anchor->id : 0; o.newText = text; o.author = author;
    LNode* nn = d->tree.insertAfter(anchor, id, text, author);
    d->staged.push(o);
    pl("staged InsertLine(" + lid(id) + ")");
    warnText(d->tree.rank(nn), text);
}
void cmdEmbed(Doc* d, const string& author, const string& name) {
    if (!validRef(name)) { err("invalid name \"" + name + "\""); return; }
    string text = "{{embed:" + name + "}}";
    if (len(text) > 200) { err("line text is longer than 200 characters"); return; }
    if (d->tree.size() >= 5000) { err("limit of 5000 lines per document reached"); return; }
    Doc* tg = docAt(absRef(d, name));
    string cyc;
    if (tg && wouldCycle(d, tg, cyc)) {
        err("embed rejected, it would create a cycle");
        pl("  " + cyc);
        return;
    }
    doInsert(d, d->tree.size() + 1, text, author);
}

// ---------------------------------------------------------------- show / authors / tac
int blockedCount(Doc* d) {
    int c = 0;
    Array<string> tk;
    for (LNode* x = d->tree.first(); x; x = LineTree::next(x)) {
        tokenize(x->text, tk);
        for (int i = 0; i < tk.size(); i++) if (blocked.find(tk[i])) { c++; break; }
    }
    return c;
}
void showDoc(Doc* d, int indent, bool top) {
    int n = 1;
    for (LNode* x = d->tree.first(); x; x = LineTree::next(x), n++) {
        string pre(indent * 4, ' ');
        if (top) pl(pre + num(n) + " [" + x->author + "] " + x->text);
        else pl(pre + "[" + x->author + "] " + x->text);
        Array<string> refs;
        scanRefs(x->text, 1, refs);
        for (int i = 0; i < refs.size(); i++) {
            Doc* t = docAt(absRef(d, refs[i]));
            if (!t || indent >= 8) continue;
            if (t->expanding) { pl(string((indent + 1) * 4, ' ') + "[embed cycle skipped]"); continue; }
            t->expanding = true;
            showDoc(t, indent + 1, false);
            t->expanding = false;
        }
    }
}
void cmdShow() {
    if (!cur) { err("no document is open"); return; }
    Doc* d = cur;
    d->expanding = true;
    showDoc(d, 0, true);
    d->expanding = false;
    pl(d->node->name + " | head " + cid(d->head->num) + " | " + num(d->tree.size()) + " lines | staged " + num(d->staged.size()) +
       " | links out " + num(d->out[0].size()) + " | backlinks " + num(d->in[0].size()) + " | blocked " + num(blockedCount(d)));
}
void cmdAuthors() {
    if (!cur) { err("no document is open"); return; }
    Array<string> names; Array<int> cnt;
    for (LNode* x = cur->tree.first(); x; x = LineTree::next(x)) {
        int k = -1;
        for (int i = 0; i < names.size(); i++) if (names[i] == x->author) k = i;
        if (k < 0) { names.push(x->author); cnt.push(1); } else cnt[k]++;
    }
    for (int i = 0; i < names.size(); i++) pl(names[i] + " " + plural(cnt[i], "line"));
}
void cmdTac(const string& a) {
    Doc* d = resolveDoc(a);
    if (!d) return;
    Stack<LNode*> st;                          // push in order, pop = reverse order
    for (LNode* x = d->tree.first(); x; x = LineTree::next(x)) st.push(x);
    while (!st.empty()) {
        LNode* x = st.pop();
        pl(num(d->tree.rank(x)) + " [" + x->author + "] " + x->text);
    }
}

// ---------------------------------------------------------------- save / discard
void bloomOps(Bloom* b, Ring<Op>& ops) {
    Array<string> tk;
    for (int i = 0; i < ops.size(); i++) {
        Op& o = ops.get(i);
        if (o.type == OP_DEL) continue;
        tokenize(o.newText, tk);
        for (int j = 0; j < tk.size(); j++) b->add(tk[j]);
    }
}
void cmdSave() {
    if (!cur) { err("no document is open"); return; }
    Doc* d = cur;
    if (d->staged.size() == 0) { pl("nothing to save"); return; }
    string cyc;
    if (!applyGraphOps(d, d->staged, cyc)) {   // embed check: one failure rejects the whole save
        err("save rejected, it would create an embed cycle");
        pl("  " + cyc);
        return;
    }
    indexOps(d, d->staged);
    Commit* c = newCommit(d->head, 0);
    int nops = d->staged.size();
    c->ops.takeFrom(d->staged);
    bloomOps(c->bloom, c->ops);
    setHead(d, c);
    d->lastMod = tick;
    pl("saved " + cid(c->num) + " (" + plural(nops, "op") + ")");
}
void cmdDiscard() {
    if (!cur) { err("no document is open"); return; }
    Doc* d = cur;
    int n = d->staged.size();
    if (n == 0) { pl("nothing to discard"); return; }
    for (int i = n - 1; i >= 0; i--) applyInv(d->tree, d->staged.get(i));   // inverse ops, newest first
    d->staged.clear();
    pl("discarded " + plural(n, "staged op"));
}
