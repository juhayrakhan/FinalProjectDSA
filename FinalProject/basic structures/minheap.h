#pragma once
#include "dynamicarray.h"

// ---------------------------------------------------------------- Bounded min-heap
struct HeapItem { double score; int visit; int arr; int b; };
struct MinHeap {
	DynamicArray<HeapItem> h; int k;
	explicit MinHeap(int kk) : k(kk) {}
	// "smaller" = toLowercase score; among equal scores the later visit is smaller
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
