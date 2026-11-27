// part_explorer.h - (included inside struct Engine) folder tree, paths, sorting, recycle bin.

// ---------------------------------------------------------------- names and paths
static bool validName(const string& s) {
    if (s.empty() || len(s) > 32) return false;
    for (int i = 0; i < len(s); i++) {
        char c = s[i];
        if (!(isAN(c) || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}
bool validAuthor(const string& s) {
    if (s.empty() || len(s) > 16) return false;
    for (int i = 0; i < len(s); i++) if (!isAN(s[i])) return false;
    return true;
}
static string joinPath(FNode* par, const string& name) { return par->path == "/" ? "/" + name : par->path + "/" + name; }
static FNode* findKid(FNode* par, const string& name) {
    for (int i = 0; i < par->kids.size(); i++) if (par->kids[i]->name == name) return par->kids[i];
    return 0;
}
static void freeFN(FNode* n) {
    for (int i = 0; i < n->kids.size(); i++) freeFN(n->kids[i]);
    delete n;
}
FNode* mkNode(FNode* par, const string& name, bool file) {
    FNode* n = new FNode;
    n->name = name; n->file = file; n->parent = par; n->path = joinPath(par, name);
    par->kids.push(n);
    paths.put(n->path, n);
    return n;
}
// splits s on '/' into seg; with dots, "." is ignored and ".." pops (false if it leaves the root)
static bool pushSegs(const string& s, Array<string>& seg, bool dots) {
    int i = 0;
    while (i <= len(s)) {
        int j = s.find('/', i);
        if (j < 0) j = len(s);
        string piece = s.substr(i, j - i);
        i = j + 1;
        if (piece.empty() || piece == ".") continue;
        if (piece == ".." && dots) { if (seg.empty()) return false; seg.pop(); continue; }
        seg.push(piece);
    }
    return true;
}
bool norm(const string& p, string& out) {            // absolute normalised path
    Array<string> seg;
    bool abs = !p.empty() && p[0] == '/';
    if (!abs) pushSegs(cwd->path, seg, false);
    if (!pushSegs(p, seg, true)) return false;
    out = "";
    for (int i = 0; i < seg.size(); i++) out += "/" + seg[i];
    if (out.empty()) out = "/";
    return true;
}
FNode* lookup(const string& p) {                  // O(1) expected through the path table
    string a;
    if (!norm(p, a)) return 0;
    FNode** x = paths.find(a);
    return x ? *x : 0;
}
Doc* docAt(const string& abs) {                   // live document at an absolute path
    FNode** x = paths.find(abs);
    return (x && (*x)->file) ? (*x)->doc : 0;
}
Doc* resolveDoc(const string& a) {
    FNode* n = lookup(a);
    if (!n) { err(a + " does not exist"); return 0; }
    if (!n->file) { err(a + " is a folder, not a document"); return 0; }
    return n->doc;
}
FNode* refFolder() { return cur ? cur->node->parent : cwd; }
string dispName(Doc* d) {                         // same folder as the current document: name; else absolute path
    return d->node->parent == refFolder() ? d->node->name : d->node->path;
}
string dispPath(const string& abs) {                 // same rule for a path that may not exist
    int k = abs.rfind('/');
    string folder = k <= 0 ? string("/") : abs.substr(0, k);
    return folder == refFolder()->path ? abs.substr(k + 1) : abs;
}

// ---------------------------------------------------------------- navigation
void cmdCd(const string& a) {
    string ab;
    if (!norm(a, ab)) {
        if (a == ".." && cwd == root) err("already at the root");
        else err(a + " does not exist");
        return;
    }
    FNode** x = paths.find(ab);
    if (!x) { err(a + " does not exist"); return; }
    if ((*x)->file) { err(a + " is not a folder"); return; }
    cwd = *x;
    cur = 0;                                   // closing a document neither saves nor discards
}
bool docLimit() {
    if (docs.size() >= 50) { err("limit of 50 documents reached"); return true; }
    return false;
}
bool nameOk(const string& name, FNode* par) {
    if (!validName(name)) { err("invalid name \"" + name + "\""); return false; }
    if (findKid(par, name)) { err(name + " already exists in " + par->path); return false; }
    return true;
}
void cmdMkdir(const string& name) {
    if (!nameOk(name, cwd)) return;
    mkNode(cwd, name, false);
}
void cmdNew(const string& name) {
    if (!nameOk(name, cwd)) return;
    if (docLimit()) return;
    FNode* n = mkNode(cwd, name, true);
    newDoc(n);
    pl("created " + name + " (F" + num(n->doc->id) + ")");
    healAll();
}
void cmdOpen(const string& a) {
    Doc* d = resolveDoc(a);
    if (d) cur = d;
}

// ---------------------------------------------------------------- listing and sorting
static string extOf(const string& n) { int k = n.rfind('.'); return k < 0 ? string("") : n.substr(k + 1); }
long docBytes(Doc* d) {
    long s = 0;
    for (LNode* x = d->tree.first(); x; x = LineTree::next(x)) s += len(x->text) + 1;
    return s;
}
long keyNum(FNode* f, int key) { return key == 1 ? (long)f->doc->lastMod : docBytes(f->doc); }
// three-way compare of two files by key (0 name, 1 modified, 2 size, 3 ext)
int keyCmp(FNode* a, FNode* b, int key) {
    if (key == 0 || key == 3) {
        string x = lower(key == 0 ? a->name : extOf(a->name)), y = lower(key == 0 ? b->name : extOf(b->name));
        return x < y ? -1 : (y < x ? 1 : 0);
    }
    long x = keyNum(a, key), y = keyNum(b, key);
    return x < y ? -1 : (y < x ? 1 : 0);
}
void cmdLs(Array<string>& t) {
    int key = 0; bool desc = false; int i = 1;
    if (i < t.size()) {
        const string& a = t[i];
        if (a == "name" || a == "modified" || a == "size" || a == "ext") {
            key = a == "name" ? 0 : a == "modified" ? 1 : a == "size" ? 2 : 3;
            i++;
            if (i < t.size() && (t[i] == "asc" || t[i] == "desc")) { desc = t[i] == "desc"; i++; }
        } else if (a == "asc" || a == "desc") { desc = a == "desc"; i++; }
    }
    if (i < t.size()) { usage("ls [name|modified|size|ext] [asc|desc]"); return; }
    Array<FNode*> st, fo, fi;
    for (int j = 0; j < cwd->kids.size(); j++) {
        FNode* k = cwd->kids[j];
        if (!k->file) fo.push(k); else if (k->starred) st.push(k); else fi.push(k);
    }
    // stable sort: descending reverses the comparison, not the list
    auto fileLess = [&](FNode* a, FNode* b) { return desc ? keyCmp(b, a, key) < 0 : keyCmp(a, b, key) < 0; };
    stableSort(st, fileLess);
    stableSort(fi, fileLess);
    stableSort(fo, [&](FNode* a, FNode* b) { return lower(a->name) < lower(b->name); });
    for (int j = 0; j < st.size(); j++) pl(fileLine(st[j]));
    for (int j = 0; j < fo.size(); j++) pl(fo[j]->name + "/");
    for (int j = 0; j < fi.size(); j++) pl(fileLine(fi[j]));
}
string fileLine(FNode* f) {
    return string(f->starred ? "* " : "") + f->name + " " + plural(f->doc->tree.size(), "line") + " " +
           num(docBytes(f->doc)) + " B t=" + num(f->doc->lastMod);
}
void cmdStar(const string& a, bool on) {
    FNode* n = lookup(a);
    if (!n) { err(a + " does not exist"); return; }
    if (!n->file) { err("only files can be starred"); return; }
    if (on && n->starred) { err(n->name + " is already starred"); return; }
    if (!on && !n->starred) { err(n->name + " is not starred"); return; }
    n->starred = on;
}
void cmdProps(const string& a) {
    FNode* n = lookup(a);
    if (!n) { err(a + " does not exist"); return; }
    if (!n->file) { err("only files have properties"); return; }
    string e = extOf(n->name);
    pl("name: " + n->name);
    pl("extension: " + (e.empty() ? string("(none)") : "." + e));
    pl("lines: " + num(n->doc->tree.size()));
    pl("size: " + num(docBytes(n->doc)) + " bytes");
    pl("last modified: t=" + num(n->doc->lastMod));
    pl(string("starred: ") + (n->starred ? "yes" : "no"));
}

// ---------------------------------------------------------------- recycle bin
static bool isAncestorOrSame(FNode* anc, FNode* n) {
    for (; n; n = n->parent) if (n == anc) return true;
    return false;
}
Doc* firstStaged(FNode* n) {
    if (n->file) return n->doc->staged.size() ? n->doc : 0;
    for (int i = 0; i < n->kids.size(); i++) { Doc* d = firstStaged(n->kids[i]); if (d) return d; }
    return 0;
}
int countDocs(FNode* n) {
    if (n->file) return 1;
    int c = 0;
    for (int i = 0; i < n->kids.size(); i++) c += countDocs(n->kids[i]);
    return c;
}
void binSubtree(FNode* n) {                    // documents leave the graphs; paths leave the table
    if (n->file) { detachGraph(n->doc); n->doc->inBin = true; }
    for (int i = 0; i < n->kids.size(); i++) binSubtree(n->kids[i]);
    paths.erase(n->path);
}
void cmdRm(const string& a) {
    FNode* n = lookup(a);
    if (!n) { err(a + " does not exist"); return; }
    if (n == root || isAncestorOrSame(n, cwd)) { err("cannot remove the root or the current folder"); return; }
    Doc* bad = firstStaged(n);
    if (bad) { err(bad->node->name + " has staged changes; save or discard first"); return; }
    BinItem* b = new BinItem;
    b->num = ++binCtr; b->node = n; b->origParent = n->parent->path; b->origName = n->name;
    binSubtree(n);
    FNode* par = n->parent;
    for (int i = 0; i < par->kids.size(); i++) if (par->kids[i] == n) { par->kids.removeAt(i); break; }
    n->parent = 0;
    bin.push(b);
    if (cur && cur->inBin) cur = 0;
    pl("moved " + n->name + (n->file ? "" : "/") + " to the recycle bin as item " + num(b->num));
}
void cmdBin() {
    if (bin.empty()) { pl("the bin is empty"); return; }
    for (int i = 0; i < bin.size(); i++) {
        BinItem* b = bin[i];
        string s = num(b->num) + " " + b->origName + (b->node->file ? "" : "/") + " from " + b->origParent;
        if (!b->node->file) s += " (" + plural(countDocs(b->node), "document") + ")";
        pl(s);
    }
}
static string restoredName(const string& nm) {
    int k = nm.rfind('.');
    if (k < 0) return nm + "_restored";
    return nm.substr(0, k) + "_restored" + nm.substr(k);
}
void relink(FNode* n) {                        // recompute paths below a re-attached node
    n->path = joinPath(n->parent, n->name);
    paths.put(n->path, n);
    if (n->file) n->doc->inBin = false;
    for (int i = 0; i < n->kids.size(); i++) relink(n->kids[i]);
}
void collectDocs(FNode* n, Array<Doc*>& out) {
    if (n->file) out.push(n->doc);
    for (int i = 0; i < n->kids.size(); i++) collectDocs(n->kids[i], out);
}
void cmdRestore(const string& a) {
    int k;
    if (!parseInt(a, k)) { usage("restore <n>"); return; }
    int bi = -1;
    for (int i = 0; i < bin.size(); i++) if (bin[i]->num == k) bi = i;
    if (bi < 0) { err("the bin has no item " + a); return; }
    BinItem* b = bin[bi];
    FNode* node = b->node;
    FNode** pp = paths.find(b->origParent);
    bool avail = pp && !(*pp)->file;
    FNode* par = avail ? *pp : root;
    string nm = b->origName; bool taken = false;
    while (findKid(par, nm)) { taken = true; nm = restoredName(nm); }
    node->name = nm; node->parent = par; par->kids.push(node);
    relink(node);
    string orig = b->origName, origPar = b->origParent;
    bin.removeAt(bi);
    delete b;
    string msg = "restored " + node->path;
    if (!avail && !taken) msg += " (its folder " + origPar + " is not available)";
    else if (!avail && taken) msg += " (its folder is not available; " + orig + " was taken)";
    else if (taken) msg += " (" + orig + " was taken)";
    pl(msg);
    Array<Doc*> ds; collectDocs(node, ds);
    stableSort(ds, [](Doc* x, Doc* y) { return x->id < y->id; });
    for (int i = 0; i < ds.size(); i++) rebuildGraph(ds[i]);   // rescan saved text, re-resolve
    healAll();
}
void killSubtree(FNode* n, int& cnt) {
    if (n->file && n->doc) { killDoc(n->doc); cnt++; }
    for (int i = 0; i < n->kids.size(); i++) killSubtree(n->kids[i], cnt);
    delete n;
}
void cmdEmptyBin() {
    if (bin.empty()) { pl("the bin is empty"); return; }
    int items = bin.size(), freed = 0;
    for (int i = 0; i < bin.size(); i++) { killSubtree(bin[i]->node, freed); delete bin[i]; }
    bin.clear();
    pl("emptied the bin: " + plural(items, "item") + " deleted permanently, " + plural(freed, "document") + " freed");
}
