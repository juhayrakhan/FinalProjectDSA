#pragma once
#include "utilityfuncs.h"
using namespace std;

template <class X>
class DynamicArray {
	void doublesize() {
		if (count != cap) return;
		if (!cap) cap = 1;
		else cap *= 2;
		X* arr2 = new X[cap];
		for (int i = 0; i < count; i++) arr2[i] = arr[i];
		delete[] arr; arr = arr2;
	}
public:
	X* arr; int count; int cap;

	DynamicArray(): arr(nullptr), count(0), cap(0) {}

	int size() const { return count; }
	bool empty() const { return !count; }
	X& operator[](int i) { return arr[i]; }

	// FIND AND FIX INSTANTIATIONS OF THIS
	const X& operator[](int i) const { return arr[i]; }
	X& last() { return arr[count - 1]; }

	void push(const X& v) {
		doublesize();
		arr[count++] = v;
	}
	X pop() { return arr[--count]; }

	void clear() { count = 0; }
	~DynamicArray() { clear(); delete[] arr; arr = nullptr; }

	void removeAt(int i) {
		for (int j = i + 1; j < count; j++) 
			arr[j - 1] = arr[j];
		count--;
	}
	void insertAt(int i, X v) {
		push(v); // making space for it
		for (int j = count - 1; j > i; j--)
			arr[j] = arr[j - 1];
		arr[i] = v;
	}
};


// ---------------------------------------------------------------- Stack
template <class X> struct Stack {
	DynamicArray<X> s;
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
	Node** b; int nb; int count; H h;
	HashMap() : nb(8), count(0) { b = new Node*[nb]; for (int i = 0; i < nb; i++) b[i] = 0; }
	~HashMap() { clear(); delete[] b; }
	void clear() {
		for (int i = 0; i < nb; i++) {
			Node* x = b[i];
			while (x) { Node* y = x->next; delete x; x = y; }
			b[i] = 0;
		}
		count = 0;
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
		*pp = nd; count++;
		V* r = &nd->val;
		if ((long)count * 4 > (long)nb * 3) rehash();   // size/buckets strictly > 0.75
		return r;
	}
	bool erase(const K& k) {
		Node** pp = &b[h(k) % (unsigned int)nb];
		while (*pp) {
			if ((*pp)->key == k) { Node* d = *pp; *pp = d->next; delete d; count--; return true; }
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
		x = count; count = o.count; o.count = x;
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
template <class X, class C> void stableSort(DynamicArray<X>& v, C lf) {
	if (v.size() < 2) return;
	X* tmp = new X[v.size()];
	msortRec(v.arr, tmp, 0, v.size(), lf);
	delete[] tmp;
}

// ---------------------------------------------------------------- Bounded min-heap
struct HeapItem { double score; int visit; int arr; int b; };
struct MinHeap {
	DynamicArray<HeapItem> h; int k;
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
		int count = h.size();
		for (;;) {
			int l = 2 * i + 1, r = l + 1, m = i;
			if (l < count && lt(h[l], h[m])) m = l;
			if (r < count && lt(h[r], h[m])) m = r;
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
		h.count--;
		if (h.size() > 0) down(0);
		return r;
	}
};

// ---------------------------------------------------------------- Text helpers
// token: maximal run of English letters/digits, lowercased (Section 2)
void tokenize(const string& s, DynamicArray<string>& out) {
	out.clear();
	int i = 0, count = s.size();
	while (i < count) {
		if (isAN(s[i])) {
			string x;
			while (i < count && isAN(s[i])) { x += lc(s[i]); i++; }
			out.push(x);
		} else i++;
	}
}
bool hasTok(DynamicArray<string>& a, const string& x) {
	for (int i = 0; i < a.size(); i++) if (a[i] == x) return true;
	return false;
}
void distinctTok(const string& s, DynamicArray<string>& out) {
	DynamicArray<string> all;
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
