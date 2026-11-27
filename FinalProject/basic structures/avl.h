// avl.h - order-statistic AVL tree holding a document's lines (Section 5.3).
// In-order sequence = line order. There are no keys: insertion is "after a node".
// The tree owns the LineID -> node hash map so both always hold the same lines.
#pragma once
#include "ds.h"

struct LNode {
    unsigned long long id; string text, author;
    LNode *l, *r, *p;
    int h, sz;
};

struct LineTree {
    LNode* root;
    HashMap<unsigned long long, LNode*, HId> ids;
    LineTree() : root(0) {}
    ~LineTree() { freeRec(root); }

    static void freeRec(LNode* n) { if (!n) return; freeRec(n->l); freeRec(n->r); delete n; }
    static int H(LNode* n) { return n ? n->h : 0; }
    static int S(LNode* n) { return n ? n->sz : 0; }
    static void upd(LNode* n) { n->h = 1 + maxxx(H(n->l), H(n->r)); n->sz = 1 + S(n->l) + S(n->r); }
    int size() const { return root ? root->sz : 0; }
    LNode* find(unsigned long long id) const { LNode** x = ids.find(id); return x ? *x : 0; }

    // every rotation updates heights, sizes and parent pointers
    LNode* rotL(LNode* x) {
        LNode* y = x->r;
        x->r = y->l; if (y->l) y->l->p = x;
        y->p = x->p;
        if (!x->p) root = y; else if (x->p->l == x) x->p->l = y; else x->p->r = y;
        y->l = x; x->p = y;
        upd(x); upd(y);
        return y;
    }
    LNode* rotR(LNode* x) {
        LNode* y = x->l;
        x->l = y->r; if (y->r) y->r->p = x;
        y->p = x->p;
        if (!x->p) root = y; else if (x->p->l == x) x->p->l = y; else x->p->r = y;
        y->r = x; x->p = y;
        upd(x); upd(y);
        return y;
    }
    // walk to the root: refresh height/size at every ancestor, rebalance where needed
    void rebalanceUp(LNode* n) {
        while (n) {
            upd(n);
            int bf = H(n->l) - H(n->r);
            if (bf > 1) {
                if (H(n->l->l) < H(n->l->r)) rotL(n->l);   // left-right case
                n = rotR(n);
            } else if (bf < -1) {
                if (H(n->r->r) < H(n->r->l)) rotR(n->r);   // right-left case
                n = rotL(n);
            }
            n = n->p;
        }
    }
    // insert a new line right after `anchor` (anchor == 0: at the top). O(log N)
    LNode* insertAfter(LNode* anchor, unsigned long long id, const string& text, const string& author) {
        LNode* x = new LNode;
        x->id = id; x->text = text; x->author = author;
        x->l = x->r = x->p = 0; x->h = 1; x->sz = 1;
        if (!root) root = x;
        else if (!anchor) { LNode* t = root; while (t->l) t = t->l; t->l = x; x->p = t; }
        else if (!anchor->r) { anchor->r = x; x->p = anchor; }
        else { LNode* t = anchor->r; while (t->l) t = t->l; t->l = x; x->p = t; }
        ids.put(id, x);
        rebalanceUp(x->p);
        return x;
    }
    void erase(LNode* n) {                   // O(log N)
        ids.erase(n->id);
        if (n->l && n->r) {                  // two children: take the successor's content
            LNode* s = n->r; while (s->l) s = s->l;
            n->id = s->id; n->text = s->text; n->author = s->author;
            ids.put(n->id, n);
            n = s;
        }
        LNode* c = n->l ? n->l : n->r;
        LNode* par = n->p;
        if (c) c->p = par;
        if (!par) root = c; else if (par->l == n) par->l = c; else par->r = c;
        delete n;
        rebalanceUp(par);
    }
    LNode* select(int k) const {             // 1-based, O(log N)
        LNode* n = root;
        while (n) {
            int ls = S(n->l);
            if (k <= ls) n = n->l;
            else if (k == ls + 1) return n;
            else { k -= ls + 1; n = n->r; }
        }
        return 0;
    }
    int rank(LNode* n) const {               // line number of a node, O(log N)
        int r = S(n->l) + 1;
        while (n->p) { if (n->p->r == n) r += S(n->p->l) + 1; n = n->p; }
        return r;
    }
    LNode* first() const { LNode* n = root; while (n && n->l) n = n->l; return n; }
    LNode* last() const { LNode* n = root; while (n && n->r) n = n->r; return n; }
    static LNode* next(LNode* n) {
        if (n->r) { n = n->r; while (n->l) n = n->l; return n; }
        while (n->p && n->p->r == n) n = n->p;
        return n->p;
    }
    void clear() { freeRec(root); root = 0; ids.clear(); }
    static LNode* clone(LNode* n, LNode* par) {
        if (!n) return 0;
        LNode* c = new LNode(*n);
        c->p = par;
        c->l = clone(n->l, c);
        c->r = clone(n->r, c);
        return c;
    }
    void copyFrom(const LineTree& o) {       // deep copy, same LineIDs
        clear();
        root = clone(o.root, 0);
        for (LNode* x = first(); x; x = next(x)) ids.put(x->id, x);
    }
    void swapWith(LineTree& o) {
        LNode* t = root; root = o.root; o.root = t;
        ids.swapWith(o.ids);
    }
    // invariant check; why = "kind|detail"
    bool verify(string& why) const {
        if (root && root->p) { why = "avl-parent|root has a parent"; return false; }
        int cnt = 0;
        if (!chk(root, why, cnt)) return false;
        if (cnt != ids.n) { why = "lineid-map|map holds " + to_string(ids.n) + " entries, tree " + to_string(cnt) + " lines"; return false; }
        return true;
    }
    bool chk(LNode* n, string& why, int& cnt) const {
        if (!n) return true;
        cnt++;
        string nm = "node L" + to_string(n->id);
        if (n->l && n->l->p != n) { why = "avl-parent|" + nm + ", left child has wrong parent"; return false; }
        if (n->r && n->r->p != n) { why = "avl-parent|" + nm + ", right child has wrong parent"; return false; }
        if (!chk(n->l, why, cnt) || !chk(n->r, why, cnt)) return false;
        int bf = H(n->l) - H(n->r);
        if (bf > 1 || bf < -1) { why = "avl-balance|" + nm + ", balance factor " + to_string(bf); return false; }
        if (n->h != 1 + maxxx(H(n->l), H(n->r))) { why = "avl-height|" + nm + ", wrong height"; return false; }
        if (n->sz != 1 + S(n->l) + S(n->r)) { why = "avl-size|" + nm + ", wrong subtree size"; return false; }
        LNode** m = ids.find(n->id);
        if (!m || *m != n) { why = "lineid-map|" + nm + " is not mapped to its node"; return false; }
        return true;
    }
};
