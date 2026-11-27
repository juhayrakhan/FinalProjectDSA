// part_core.h - (included inside struct Engine) output, parsing, dispatch, run loop.

// ---------------------------------------------------------------- output helpers
void p(const string& s) { if (!quiet) cout << s; }
void pl(const string& s) { if (!quiet) cout << s << "\n"; }
void err(const string& s) { pl("error: " + s); }
void usage(const string& s) { err("usage: " + s); }
static string num(long v) { return to_string(v); }
static string plural(long n, const char* w) { return to_string(n) + " " + w + (n == 1 ? "" : "s"); }
static string lid(unsigned long long v) { return "L" + to_string(v); }
static string cid(unsigned int v) { return "c" + to_string(v); }

// ---------------------------------------------------------------- breadcrumb (Section 5.2.2)
string crumb() {
    Array<string> rev;
    for (FNode* n = cwd; n != root; n = n->parent) rev.push(n->name);
    Array<string> seg;
    seg.push("root");
    for (int i = rev.size() - 1; i >= 0; i--) seg.push(rev[i]);
    if (cur) seg.push(cur->node->name);
    string full;
    for (int i = 0; i < seg.size(); i++) { if (i) full += " > "; full += seg[i]; }
    if (len(full) <= 40) return full;
    // "root > ... > " is 13 characters; keep as many trailing segments as still fit in 40
    int best = 1;
    for (int k = 1; k < seg.size(); k++) {
        int L = 0;
        for (int i = seg.size() - k; i < seg.size(); i++) { if (i > seg.size() - k) L += 3; L += len(seg[i]); }
        if (13 + L <= 40) best = k; else break;
    }
    string r = "root > ... > ";
    for (int i = seg.size() - best; i < seg.size(); i++) { if (i > seg.size() - best) r += " > "; r += seg[i]; }
    return r;
}

// ---------------------------------------------------------------- command line parsing
// words are split on blanks; a word that starts with '"' begins the quoted tail
void split(const string& s, Array<string>& t, string& tail) {
    int i = 0, n = len(s);
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i++;
        if (i >= n) break;
        if (s[i] == '"') {
            tail = s.substr(i);
            while (!tail.empty() && (tail[tail.size() - 1] == ' ' || tail[tail.size() - 1] == '\t')) tail.erase(tail.size() - 1);
            return;
        }
        int j = i;
        while (j < n && s[j] != ' ' && s[j] != '\t' && s[j] != '\r') j++;
        t.push(s.substr(i, j - i));
        i = j;
    }
}
// 0 ok, 1 not inside double quotes, 2 longer than 200 characters
int parseQuoted(const string& tail, string& out) {
    if (tail.empty() || tail[0] != '"') return 1;
    string r; int i = 1; bool closed = false;
    while (i < len(tail)) {
        char c = tail[i];
        if (c == '\\' && i + 1 < len(tail) && tail[i + 1] == '"') { r += '"'; i += 2; continue; }
        if (c == '"') { closed = true; i++; break; }
        r += c; i++;
    }
    if (!closed || i < len(tail)) return 1;
    if (len(r) > 200) return 2;
    out = r;
    return 0;
}
bool textArg(const string& tail, string& out) {
    int r = parseQuoted(tail, out);
    if (r == 1) { err("text must be inside double quotes"); return false; }
    if (r == 2) { err("line text is longer than 200 characters"); return false; }
    return true;
}

// ---------------------------------------------------------------- dispatch
void exec(const string& line) {
    tick++;                                   // now(): one tick per command line
    bool wasOn = checkOn;
    Array<string> t; string tail;
    split(line, t, tail);
    bool isCheckCmd = false;
    if (t.size() == 0) {
        if (!tail.empty()) err("unknown command \"" + tail + "\"");
    } else {
        const string& c = t[0];
        int na = t.size() - 1 + (tail.empty() ? 0 : 1);
        if (c == "check") isCheckCmd = true;
        if (c == "exit") isCheckCmd = true;
        if (c == "pwd") { if (na) usage("pwd"); else pl(cwd->path); }
        else if (c == "cd") { if (na != 1 || !tail.empty()) usage("cd <path>"); else cmdCd(t[1]); }
        else if (c == "ls") { if (!tail.empty()) usage("ls [name|modified|size|ext] [asc|desc]"); else cmdLs(t); }
        else if (c == "mkdir") { if (na != 1 || !tail.empty()) usage("mkdir <name>"); else cmdMkdir(t[1]); }
        else if (c == "new") { if (na != 1 || !tail.empty()) usage("new <name>"); else cmdNew(t[1]); }
        else if (c == "open") { if (na != 1 || !tail.empty()) usage("open <name>"); else cmdOpen(t[1]); }
        else if (c == "star") { if (na != 1 || !tail.empty()) usage("star <name>"); else cmdStar(t[1], true); }
        else if (c == "unstar") { if (na != 1 || !tail.empty()) usage("unstar <name>"); else cmdStar(t[1], false); }
        else if (c == "props") { if (na != 1 || !tail.empty()) usage("props <name>"); else cmdProps(t[1]); }
        else if (c == "rm") { if (na != 1 || !tail.empty()) usage("rm <name>"); else cmdRm(t[1]); }
        else if (c == "bin") { if (na) usage("bin"); else cmdBin(); }
        else if (c == "restore") { if (na != 1 || !tail.empty()) usage("restore <n>"); else cmdRestore(t[1]); }
        else if (c == "emptybin") { if (na) usage("emptybin"); else cmdEmptyBin(); }
        else if (c == "as") cmdAs(t, tail);
        else if (c == "show") { if (na) usage("show"); else cmdShow(); }
        else if (c == "authors") { if (na) usage("authors"); else cmdAuthors(); }
        else if (c == "save") { if (na) usage("save"); else cmdSave(); }
        else if (c == "discard") { if (na) usage("discard"); else cmdDiscard(); }
        else if (c == "tac") { if (na != 1 || !tail.empty()) usage("tac <name>"); else cmdTac(t[1]); }
        else if (c == "log") { if (na) usage("log"); else cmdLog(); }
        else if (c == "view") { if (na != 1 || !tail.empty()) usage("view <commit>"); else cmdView(t[1]); }
        else if (c == "fork") { if (na != 2 || !tail.empty()) usage("fork <doc> <new-name>"); else cmdFork(t[1], t[2]); }
        else if (c == "common") { if (na != 2 || !tail.empty()) usage("common <doc1> <doc2>"); else cmdCommon(t[1], t[2]); }
        else if (c == "merge") {
            if (na != 3 || !tail.empty() || t[2] != "into") usage("merge <incoming> into <current>");
            else cmdMerge(t[1], t[3]);
        }
        else if (c == "search") { if (na != 1 || !tail.empty()) usage("search <word> or search <prefix>*"); else cmdSearch(t[1]); }
        else if (c == "backlinks") { if (na) usage("backlinks"); else cmdBacklinks(); }
        else if (c == "related") { if (na != 1 || !tail.empty()) usage("related <h>"); else cmdRelated(t[1]); }
        else if (c == "broken") { if (na) usage("broken"); else cmdBroken(); }
        else if (c == "orphans") { if (na) usage("orphans"); else cmdOrphans(); }
        else if (c == "similar") { if (na < 1 || na > 2 || !tail.empty()) usage("similar <k> [percent]"); else cmdSimilar(t); }
        else if (c == "block") { if (na != 1 || !tail.empty()) usage("block <word>"); else cmdBlock(t[1]); }
        else if (c == "unblock") { if (na != 1 || !tail.empty()) usage("unblock <word>"); else cmdUnblock(t[1]); }
        else if (c == "blocked") { if (na) usage("blocked"); else cmdBlocked(); }
        else if (c == "fptest") { if (na != 4 || !tail.empty()) usage("fptest <bloom|counting> <m> <k> <n>"); else cmdFptest(t[1], t[2], t[3], t[4]); }
        else if (c == "check") { if (na != 1 || !tail.empty()) usage("check on|off"); else cmdCheck(t[1]); }
        else if (c == "fuzz") { if (na != 2 || !tail.empty()) usage("fuzz <seed> <count>"); else cmdFuzz(t[1], t[2]); }
        else if (c == "run") { if (na != 1 || !tail.empty()) usage("run <file>"); else cmdRun(t[1]); }
        else if (c == "exit") { if (na) usage("exit"); else { if (checkOn) finishCheck(); exiting = true; } }
        else err("unknown command \"" + c + "\"");
    }
    if (wasOn && checkOn && !isCheckCmd && !halted) { checkCount++; runCheck(); }
}

// reads one merge answer line from the active input source
bool readAnswer(string& a) {
    if (!in) return false;
    while (in->line(a)) {
        int s = 0;
        while (s < len(a) && (a[s] == ' ' || a[s] == '\t')) s++;
        string tr = a.substr(s);
        while (!tr.empty() && (tr[tr.size() - 1] == ' ' || tr[tr.size() - 1] == '\t')) tr.erase(tr.size() - 1);
        if (script && (tr.empty() || tr[0] == '#')) continue;   // skipped silently, no tick
        a = tr;
        if (script) pl(a);                                      // script runs echo the answer
        return true;
    }
    return false;
}

void runLoop(Input& src) {
    Input* saveIn = in; bool saveScript = script;
    in = &src; script = src.isScript();
    string line;
    while (!exiting && !halted) {
        if (!script) { p(crumb() + " $ "); if (!quiet) cout.flush(); }
        if (!src.line(line)) { if (!script) pl(""); exiting = script ? exiting : true; break; }
        int s = 0;
        while (s < len(line) && (line[s] == ' ' || line[s] == '\t')) s++;
        if (s >= len(line) || line[s] == '#') continue;           // blank / comment: no echo, no tick
        if (script) pl(crumb() + " $ " + line);
        exec(line);
    }
    in = saveIn; script = saveScript;
}

void cmdRun(const string& path) {
    FileInput f(path);
    if (!f.ok) { err("cannot open file " + path); return; }
    runLoop(f);
}
