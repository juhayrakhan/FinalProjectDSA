// ds.h - hand-written foundations (Section 5.1). No STL containers/algorithms.
#pragma once
#include <string>
#include <cstddef>
#include <cmath> 
using namespace std;

int len(const string& s) { return s.size(); }
bool isAL(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDG(char c) { return c >= '0' && c <= '9'; }
bool isAN(char c) { return isAL(c) || isDG(c); }
char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
string lower(const string& s) {
    string r = s;
    for (int i = 0; i < r.size(); i++) r[i] = lc(r[i]);
    return r;
}
int maxxx(int a, int b) { return a > b ? a : b; }

// ---------------------------------------------------------------- Dynamic array
template <class X> struct Array {
    X* arr; int n; int cap;
    Array() : arr(nullptr), n(0), cap(0) {}
    ~Array() { delete[] arr; }
    int size() const { return n; }
    bool empty() const { return n == 0; }
    X& operator[](int i) { return arr[i]; }
    const X& operator[](int i) const { return arr[i]; }
    X& last() { return arr[n - 1]; }
    // doubles the capacity when full: amortised O(1)
    void push(const X& v) {
        if (n == cap) {
            int nc = cap ? cap * 2 : 4;
            X* b = new X[nc];
            for (int i = 0; i < n; i++) b[i] = arr[i];
            b[n] = v;                 // copy before freeing (v may alias an element)
            delete[] arr; arr = b; cap = nc; n++;
        } else arr[n++] = v;
    }
    X pop() { return arr[--n]; }
    void clear() { n = 0; }
    void removeAt(int i) { for (int j = i + 1; j < n; j++) arr[j - 1] = arr[j]; n--; }
    void insertAt(int i, const X& v) {
        X tmp = v;
        push(tmp);
        for (int j = n - 1; j > i; j--) arr[j] = arr[j - 1];
        arr[i] = tmp;
    }
};

// ---------------------------------------------------------------- Ring queue
template <class X> struct Ring {
    X* arr; int cap; int head; int n;
    Ring() : arr(0), cap(0), head(0), n(0) {}
    ~Ring() { delete[] arr; }
    int size() const { return n; }
    void push(const X& v) {
        if (n == cap) {                      // full: double, copy in front-to-back order
            X tmp = v;
            int nc = cap ? cap * 2 : 4;
            X* b = new X[nc];
            for (int i = 0; i < n; i++) b[i] = arr[(head + i) % cap];
            delete[] arr; arr = b; cap = nc; head = 0;
            arr[n++] = tmp;
        } else { arr[(head + n) % cap] = v; n++; }
    }
    X pop() { X r = arr[head]; head = (head + 1) % cap; n--; return r; }
    X& get(int i) { return arr[(head + i) % cap]; }
    void clear() { head = 0; n = 0; }
    void takeFrom(Ring& o) {                 // steals o's storage (used by save/merge)
        delete[] arr;
        arr = o.arr; cap = o.cap; head = o.head; n = o.n;
        o.arr = 0; o.cap = 0; o.head = 0; o.n = 0;
    }
};

// ---------------------------------------------------------------- Stack
template <class X> struct Stack {
    Array<X> s;
    void push(const X& v) { s.push(v); }
    X pop() { return s.pop(); }
    bool empty() const { return s.empty(); }
    int size() const { return s.size(); }
};

// ---------------------------------------------------------------- Hashing
unsigned int fnv1a(const string& s) {
    unsigned int h = 2166136261u;
    for (int i = 0; i < s.size(); i++) { h ^= (unsigned char)s[i]; h *= 16777619u; }
    return h;
}
unsigned int djb2(const string& s) {
    unsigned int h = 5381u;
    for (int i = 0; i < s.size(); i++) h = h * 33u + (unsigned char)s[i];
    return h;
}
struct HId { unsigned int operator()(unsigned long long k) const { return (unsigned int)((k * 2654435761ULL) & 0xFFFFFFFFULL); } };
struct HStr { unsigned int operator()(const string& s) const { return fnv1a(s); } };

// ---------------------------------------------------------------- Hash map (separate chaining)
template <class K, class V, class H> struct HashMap {
    struct Node { K key; V val; Node* next; };
    Node** b; int nb; int n; H h;
    HashMap() : nb(8), n(0) { b = new Node*[nb]; for (int i = 0; i < nb; i++) b[i] = 0; }
    ~HashMap() { clear(); delete[] b; }
    void clear() {
        for (int i = 0; i < nb; i++) {
            Node* x = b[i];
            while (x) { Node* y = x->next; delete x; x = y; }
            b[i] = 0;
        }
        n = 0;
    }
    V* find(const K& k) const {
        for (Node* x = b[h(k) % (unsigned int)nb]; x; x = x->next) if (x->key == k) return &x->val;
        return 0;
    }
    // inserts (new entries go to the END of their chain) or overwrites
    V* put(const K& k, const V& v) {
        Node** pp = &b[h(k) % (unsigned int)nb];
        while (*pp) {
            if ((*pp)->key == k) { (*pp)->val = v; return &(*pp)->val; }
            pp = &(*pp)->next;
        }
        Node* nd = new Node;
        nd->key = k; nd->val = v; nd->next = 0;
        *pp = nd; n++;
        V* r = &nd->val;
        if ((long)n * 4 > (long)nb * 3) rehash();   // size/buckets strictly > 0.75
        return r;
    }
    bool erase(const K& k) {
        Node** pp = &b[h(k) % (unsigned int)nb];
        while (*pp) {
            if ((*pp)->key == k) { Node* d = *pp; *pp = d->next; delete d; n--; return true; }
            pp = &(*pp)->next;
        }
        return false;
    }
    void rehash() {
        int nb2 = nb * 2;
        Node** nbk = new Node*[nb2];
        for (int i = 0; i < nb2; i++) nbk[i] = 0;
        for (int i = 0; i < nb; i++) {
            Node* x = b[i];
            while (x) {
                Node* nx = x->next;
                x->next = 0;
                Node** pp = &nbk[h(x->key) % (unsigned int)nb2];
                while (*pp) pp = &(*pp)->next;
                *pp = x;
                x = nx;
            }
        }
        delete[] b; b = nbk; nb = nb2;
    }
    void swapWith(HashMap& o) {
        Node** tb = b; b = o.b; o.b = tb;
        int x = nb; nb = o.nb; o.nb = x;
        x = n; n = o.n; o.n = x;
    }
};

// ---------------------------------------------------------------- Stable merge sort
// less(x, y) must mean "x strictly before y". Equal items keep their order.
template <class X, class C> void msortRec(X* arr, X* x, int lo, int hi, C lf) {
    if (hi - lo < 2) return;
    int mid = (lo + hi) / 2;
    msortRec(arr, x, lo, mid, lf);
    msortRec(arr, x, mid, hi, lf);
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) { if (lf(arr[j], arr[i])) x[k++] = arr[j++]; else x[k++] = arr[i++]; }
    while (i < mid) x[k++] = arr[i++];
    while (j < hi) x[k++] = arr[j++];
    for (k = lo; k < hi; k++) arr[k] = x[k];
}
template <class X, class C> void stableSort(Array<X>& v, C lf) {
    if (v.size() < 2) return;
    X* tmp = new X[v.size()];
    msortRec(v.arr, tmp, 0, v.size(), lf);
    delete[] tmp;
}

// ---------------------------------------------------------------- Bounded min-heap
struct HeapItem { double score; int visit; int arr; int b; };
struct MinHeap {
    Array<HeapItem> h; int k;
    explicit MinHeap(int kk) : k(kk) {}
    // "smaller" = lower score; among equal scores the later visit is smaller
    static bool lt(const HeapItem& x, const HeapItem& y) {
        return x.score < y.score || (x.score == y.score && x.visit > y.visit);
    }
    void up(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (!lt(h[i], h[p])) break;
            HeapItem x = h[i]; h[i] = h[p]; h[p] = x; i = p;
        }
    }
    void down(int i) {
        int n = h.size();
        for (;;) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < n && lt(h[l], h[m])) m = l;
            if (r < n && lt(h[r], h[m])) m = r;
            if (m == i) break;
            HeapItem x = h[i]; h[i] = h[m]; h[m] = x; i = m;
        }
    }
    void offer(const HeapItem& it) {
        if (h.size() < k) { h.push(it); up(h.size() - 1); }
        else if (k > 0 && it.score > h[0].score) { h[0] = it; down(0); }
    }
    HeapItem popMin() {
        HeapItem r = h[0];
        h[0] = h[h.size() - 1];
        h.n--;
        if (h.size() > 0) down(0);
        return r;
    }
};

// ---------------------------------------------------------------- Text helpers
// token: maximal run of English letters/digits, lowercased (Section 2)
void tokenize(const string& s, Array<string>& out) {
    out.clear();
    int i = 0, n = s.size();
    while (i < n) {
        if (isAN(s[i])) {
            string x;
            while (i < n && isAN(s[i])) { x += lc(s[i]); i++; }
            out.push(x);
        } else i++;
    }
}
bool hasTok(Array<string>& a, const string& x) {
    for (int i = 0; i < a.size(); i++) if (a[i] == x) return true;
    return false;
}
void distinctTok(const string& s, Array<string>& out) {
    Array<string> all;
    tokenize(s, all);
    out.clear();
    for (int i = 0; i < all.size(); i++) if (!hasTok(out, all[i])) out.push(all[i]);
}
// percentage rounded to a whole number, halves up
int pctRound(long num, long den) { return den <= 0 ? 0 : ((200 * num + den) / (2 * den)); }
bool parseInt(const string& s, int& out) {
    if (s.empty() || s.size() > 9) return false;
    int v = 0;
    for (int i = 0; i < len(s); i++) { if (!isDG(s[i])) return false; v = v * 10 + (s[i] - '0'); }
    out = v;
    return true;
}
