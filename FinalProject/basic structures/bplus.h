// bplus.h - B+ tree mapping token -> sorted posting list (Section 5.7.1).
// Order m (runtime value, >= 3): internal nodes hold <= m children (m-1 keys) and
// >= ceil(m/2) children; leaves hold <= m-1 keys and >= ceil((m-1)/2). Leaves are linked.
// Leaf split copies the first key of the new right leaf up; internal split moves the middle key up.
#pragma once
#include "ds.h"

// Order m is a compile-time constant (>= 3). Fuzz runs construct the tree with order 3.
constexpr int BP_ORDER = 4;
static_assert(BP_ORDER >= 3, "B+ tree order must be at least 3");
constexpr int BP_FUZZ_ORDER = 3;

struct Posting { unsigned int file; unsigned long long line; };
int cmpPost(const Posting& a, const Posting& b) {
    if (a.file != b.file) return a.file < b.file ? -1 : 1;
    if (a.line != b.line) return a.line < b.line ? -1 : 1;
    return 0;
}
struct KP { string key; Posting p; };

struct BNode {
    bool leaf; int nk;
    string* keys;
    BNode** ch;                  // internal only
    Array<Posting>** post;       // leaf only (one list per key)
    BNode* next;                 // leaf link
};

struct BPlus {
    int m; BNode* root;
    struct Split { bool did; string key; BNode* right; };

    explicit BPlus(int order) : m(order), root(0) {}
    ~BPlus() { freeRec(root); }
    BPlus(const BPlus&) = delete;
    BPlus& operator=(const BPlus&) = delete;

    int minLeaf() const { return m / 2; }          // ceil((m-1)/2)
    int minCh() const { return (m + 1) / 2; }      // ceil(m/2)

    BNode* mk(bool leaf) {
        BNode* n = new BNode;
        n->leaf = leaf; n->nk = 0; n->next = 0; n->ch = 0; n->post = 0;
        n->keys = new string[m + 2];
        if (leaf) { n->post = new Array<Posting>*[m + 2]; for (int i = 0; i < m + 2; i++) n->post[i] = 0; }
        else { n->ch = new BNode*[m + 3]; for (int i = 0; i < m + 3; i++) n->ch[i] = 0; }
        return n;
    }
    void freeNode(BNode* n) { delete[] n->keys; delete[] n->ch; delete[] n->post; delete n; }
    void freeRec(BNode* n) {
        if (!n) return;
        if (n->leaf) { for (int i = 0; i < n->nk; i++) delete n->post[i]; }
        else { for (int i = 0; i <= n->nk; i++) freeRec(n->ch[i]); }
        freeNode(n);
    }

    // ---------------------------------------------------------------- search
    BNode* leafFor(const string& key) const {
        BNode* n = root;
        while (n && !n->leaf) {
            int i = 0;
            while (i < n->nk && !(key < n->keys[i])) i++;
            n = n->ch[i];
        }
        return n;
    }
    const Array<Posting>* get(const string& key) const {
        BNode* l = leafFor(key);
        if (!l) return 0;
        for (int i = 0; i < l->nk; i++) if (l->keys[i] == key) return l->post[i];
        return 0;
    }
    BNode* firstLeaf() const { BNode* n = root; while (n && !n->leaf) n = n->ch[0]; return n; }

    // ---------------------------------------------------------------- insert
    void insert(const string& key, const Posting& p) {
        if (!root) root = mk(true);
        Split s = ins(root, key, p);
        if (s.did) {
            BNode* r = mk(false);
            r->ch[0] = root; r->ch[1] = s.right; r->keys[0] = s.key; r->nk = 1;
            root = r;
        }
    }
    Split ins(BNode* n, const string& key, const Posting& p) {
        Split res; res.did = false; res.right = 0;
        if (n->leaf) {
            int i = 0;
            while (i < n->nk && n->keys[i] < key) i++;
            if (i < n->nk && n->keys[i] == key) {
                Array<Posting>& L = *n->post[i];
                int j = 0;
                while (j < L.size() && cmpPost(L[j], p) < 0) j++;
                if (j < L.size() && cmpPost(L[j], p) == 0) return res;
                L.insertAt(j, p);
                return res;
            }
            for (int j = n->nk; j > i; j--) { n->keys[j] = n->keys[j - 1]; n->post[j] = n->post[j - 1]; }
            n->keys[i] = key;
            n->post[i] = new Array<Posting>;
            n->post[i]->push(p);
            n->nk++;
            if (n->nk >= m) {                          // overflow: split, copy first key up
                int left = n->nk / 2;
                BNode* r = mk(true);
                for (int j = left; j < n->nk; j++) { r->keys[j - left] = n->keys[j]; r->post[j - left] = n->post[j]; n->post[j] = 0; }
                r->nk = n->nk - left; n->nk = left;
                r->next = n->next; n->next = r;
                res.did = true; res.key = r->keys[0]; res.right = r;
            }
            return res;
        }
        int i = 0;
        while (i < n->nk && !(key < n->keys[i])) i++;
        Split cs = ins(n->ch[i], key, p);
        if (cs.did) {
            for (int j = n->nk; j > i; j--) n->keys[j] = n->keys[j - 1];
            for (int j = n->nk + 1; j > i + 1; j--) n->ch[j] = n->ch[j - 1];
            n->keys[i] = cs.key; n->ch[i + 1] = cs.right; n->nk++;
            if (n->nk >= m) {                          // overflow: middle key moves up
                int mid = n->nk / 2;
                BNode* r = mk(false);
                r->nk = n->nk - mid - 1;
                for (int j = 0; j < r->nk; j++) r->keys[j] = n->keys[mid + 1 + j];
                for (int j = 0; j <= r->nk; j++) r->ch[j] = n->ch[mid + 1 + j];
                res.did = true; res.key = n->keys[mid]; res.right = r;
                n->nk = mid;
            }
        }
        return res;
    }

    // ---------------------------------------------------------------- delete
    void remove(const string& key, const Posting& p) {
        if (!root) return;
        rem(root, key, p);
        if (root->leaf && root->nk == 0) { freeNode(root); root = 0; }
        else if (!root->leaf && root->nk == 0) { BNode* c = root->ch[0]; freeNode(root); root = c; }
    }
    void rem(BNode* n, const string& key, const Posting& p) {
        if (n->leaf) {
            int i = 0;
            while (i < n->nk && n->keys[i] < key) i++;
            if (i >= n->nk || !(n->keys[i] == key)) return;
            Array<Posting>& L = *n->post[i];
            int j = 0;
            while (j < L.size() && cmpPost(L[j], p) != 0) j++;
            if (j >= L.size()) return;
            L.removeAt(j);
            if (L.empty()) {
                delete n->post[i];
                for (int q = i + 1; q < n->nk; q++) { n->keys[q - 1] = n->keys[q]; n->post[q - 1] = n->post[q]; }
                n->nk--;
            }
            return;
        }
        int i = 0;
        while (i < n->nk && !(key < n->keys[i])) i++;
        rem(n->ch[i], key, p);
        fix(n, i);
    }
    void dropKeyChild(BNode* n, int ki, int ci) {   // remove keys[ki] and ch[ci] from n
        for (int j = ki + 1; j < n->nk; j++) n->keys[j - 1] = n->keys[j];
        for (int j = ci + 1; j <= n->nk; j++) n->ch[j - 1] = n->ch[j];
        n->nk--;
    }
    // repair child i of n if it underflowed: borrow from a sibling, else merge
    void fix(BNode* n, int i) {
        BNode* c = n->ch[i];
        BNode* L = i > 0 ? n->ch[i - 1] : 0;
        BNode* R = i < n->nk ? n->ch[i + 1] : 0;
        if (c->leaf) {
            if (c->nk >= minLeaf()) return;
            if (L && L->nk > minLeaf()) {
                for (int j = c->nk; j > 0; j--) { c->keys[j] = c->keys[j - 1]; c->post[j] = c->post[j - 1]; }
                c->keys[0] = L->keys[L->nk - 1]; c->post[0] = L->post[L->nk - 1];
                L->post[L->nk - 1] = 0; L->nk--; c->nk++;
                n->keys[i - 1] = c->keys[0];
            } else if (R && R->nk > minLeaf()) {
                c->keys[c->nk] = R->keys[0]; c->post[c->nk] = R->post[0]; c->nk++;
                for (int j = 1; j < R->nk; j++) { R->keys[j - 1] = R->keys[j]; R->post[j - 1] = R->post[j]; }
                R->post[R->nk - 1] = 0; R->nk--;
                n->keys[i] = R->keys[0];
            } else if (L) {                            // merge c into L
                for (int j = 0; j < c->nk; j++) { L->keys[L->nk + j] = c->keys[j]; L->post[L->nk + j] = c->post[j]; c->post[j] = 0; }
                L->nk += c->nk; L->next = c->next;
                freeNode(c);
                dropKeyChild(n, i - 1, i);
            } else if (R) {                            // merge R into c
                for (int j = 0; j < R->nk; j++) { c->keys[c->nk + j] = R->keys[j]; c->post[c->nk + j] = R->post[j]; R->post[j] = 0; }
                c->nk += R->nk; c->next = R->next;
                freeNode(R);
                dropKeyChild(n, i, i + 1);
            }
            return;
        }
        if (c->nk + 1 >= minCh()) return;
        if (L && L->nk + 1 > minCh()) {
            for (int j = c->nk; j > 0; j--) c->keys[j] = c->keys[j - 1];
            for (int j = c->nk + 1; j > 0; j--) c->ch[j] = c->ch[j - 1];
            c->keys[0] = n->keys[i - 1]; c->ch[0] = L->ch[L->nk];
            n->keys[i - 1] = L->keys[L->nk - 1];
            L->nk--; c->nk++;
        } else if (R && R->nk + 1 > minCh()) {
            c->keys[c->nk] = n->keys[i]; c->ch[c->nk + 1] = R->ch[0]; c->nk++;
            n->keys[i] = R->keys[0];
            for (int j = 1; j < R->nk; j++) R->keys[j - 1] = R->keys[j];
            for (int j = 1; j <= R->nk; j++) R->ch[j - 1] = R->ch[j];
            R->nk--;
        } else if (L) {                                // merge c into L, pulling the separator down
            L->keys[L->nk] = n->keys[i - 1];
            for (int j = 0; j < c->nk; j++) L->keys[L->nk + 1 + j] = c->keys[j];
            for (int j = 0; j <= c->nk; j++) L->ch[L->nk + 1 + j] = c->ch[j];
            L->nk += 1 + c->nk;
            freeNode(c);
            dropKeyChild(n, i - 1, i);
        } else if (R) {
            c->keys[c->nk] = n->keys[i];
            for (int j = 0; j < R->nk; j++) c->keys[c->nk + 1 + j] = R->keys[j];
            for (int j = 0; j <= R->nk; j++) c->ch[c->nk + 1 + j] = R->ch[j];
            c->nk += 1 + R->nk;
            freeNode(R);
            dropKeyChild(n, i, i + 1);
        }
    }

    // ---------------------------------------------------------------- range scan / bulk
    static bool startsWith(const string& k, const string& pre) {
        if (len(k) < len(pre)) return false;
        for (int i = 0; i < len(pre); i++) if (k[i] != pre[i]) return false;
        return true;
    }
    // appends every posting of every key that starts with `pre` (walks the leaf links)
    void scanPrefix(const string& pre, Array<Posting>& out) const {
        BNode* l = leafFor(pre);
        int pos = 0;
        if (l) while (pos < l->nk && l->keys[pos] < pre) pos++;
        while (l) {
            for (int i = pos; i < l->nk; i++) {
                if (!startsWith(l->keys[i], pre)) return;
                for (int j = 0; j < l->post[i]->size(); j++) out.push((*l->post[i])[j]);
            }
            l = l->next; pos = 0;
        }
    }
    void collectFile(unsigned int file, Array<KP>& out) const {
        for (BNode* l = firstLeaf(); l; l = l->next)
            for (int i = 0; i < l->nk; i++)
                for (int j = 0; j < l->post[i]->size(); j++)
                    if ((*l->post[i])[j].file == file) { KP kp; kp.key = l->keys[i]; kp.p = (*l->post[i])[j]; out.push(kp); }
    }
    void purgeFile(unsigned int file) {
        Array<KP> v; collectFile(file, v);
        for (int i = 0; i < v.size(); i++) remove(v[i].key, v[i].p);
    }
    void copyFile(unsigned int from, unsigned int to) {
        Array<KP> v; collectFile(from, v);
        for (int i = 0; i < v.size(); i++) { Posting p = v[i].p; p.file = to; insert(v[i].key, p); }
    }

    // ---------------------------------------------------------------- invariants
    bool verify(string& why) const {
        if (!root) return true;
        Array<BNode*> leaves; int depth = -1;
        if (!vrec(root, 0, depth, 0, 0, leaves, why)) return false;
        for (int i = 0; i + 1 < leaves.size(); i++)
            if (leaves[i]->next != leaves[i + 1]) { why = "leaf link does not point to the next leaf"; return false; }
        if (leaves.size() && leaves[leaves.size() - 1]->next) { why = "last leaf has a next link"; return false; }
        return true;
    }
    bool vrec(BNode* n, int d, int& depth, const string* lo, const string* hi, Array<BNode*>& leaves, string& why) const {
        bool isRoot = (n == root);
        for (int i = 0; i < n->nk; i++) {
            if (i && !(n->keys[i - 1] < n->keys[i])) { why = "keys out of order in a node"; return false; }
            if (lo && n->keys[i] < *lo) { why = "key '" + n->keys[i] + "' below its subtree range"; return false; }
            if (hi && !(n->keys[i] < *hi)) { why = "key '" + n->keys[i] + "' above its subtree range"; return false; }
        }
        if (n->leaf) {
            if (n->nk > m - 1) { why = "leaf overfull"; return false; }
            if (!isRoot && n->nk < minLeaf()) { why = "leaf underfull"; return false; }
            for (int i = 0; i < n->nk; i++) {
                Array<Posting>& L = *n->post[i];
                if (L.empty()) { why = "empty posting list for '" + n->keys[i] + "'"; return false; }
                for (int j = 1; j < L.size(); j++)
                    if (cmpPost(L[j - 1], L[j]) >= 0) { why = "postings of '" + n->keys[i] + "' not sorted"; return false; }
            }
            if (depth < 0) depth = d;
            else if (depth != d) { why = "leaves at different depths"; return false; }
            leaves.push(n);
            return true;
        }
        if (n->nk + 1 > m) { why = "internal node overfull"; return false; }
        if (!isRoot && n->nk + 1 < minCh()) { why = "internal node underfull"; return false; }
        if (isRoot && n->nk < 1) { why = "internal root with a single child"; return false; }
        for (int i = 0; i <= n->nk; i++) {
            const string* l2 = i > 0 ? &n->keys[i - 1] : lo;
            const string* h2 = i < n->nk ? &n->keys[i] : hi;
            if (!vrec(n->ch[i], d + 1, depth, l2, h2, leaves, why)) return false;
        }
        return true;
    }
};
