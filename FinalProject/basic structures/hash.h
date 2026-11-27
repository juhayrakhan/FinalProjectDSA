#pragma once
#include <string>
using namespace std;

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
	HashMap() : nb(8), count(0) { b = new Node * [nb]; for (int i = 0; i < nb; i++) b[i] = 0; }
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
		Node** nbk = new Node * [nb2];
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