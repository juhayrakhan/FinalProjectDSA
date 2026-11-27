#pragma once


template <class X> 
struct CircularQueue {
	X* arr; int cap; int head; int count;
	CircularQueue() : arr(0), cap(0), head(0), count(0) {}
	~CircularQueue() { delete[] arr; }
	int size() const { return count; }
	void push(const X& v) {
		if (count == cap) {                      // full: double, copy in front-to-back order
			X tmp = v;
			int nc = cap ? cap * 2 : 4;
			X* b = new X[nc];
			for (int i = 0; i < count; i++) b[i] = arr[(head + i) % cap];
			delete[] arr; arr = b; cap = nc; head = 0;
			arr[count++] = tmp;
		}
		else { arr[(head + count) % cap] = v; count++; }
	}
	X pop() { X r = arr[head]; head = (head + 1) % cap; count--; return r; }
	X& get(int i) { return arr[(head + i) % cap]; }
	void clear() { head = 0; count = 0; }
	void takeFrom(CircularQueue& o) {                 // steals o's storage (used by save/merge)
		delete[] arr;
		arr = o.arr; cap = o.cap; head = o.head; count = o.count;
		o.arr = 0; o.cap = 0; o.head = 0; o.count = 0;
	}
};