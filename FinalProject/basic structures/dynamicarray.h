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


/// STUFF AHEAD OF THIS MUST BE MOVED


template <class X>
struct Stack {
	DynamicArray<X> arr;
	void push(const X& v) { arr.push(v); }
	X pop() { return arr.pop(); }
	bool empty() const { return arr.empty(); }
	int size() const { return arr.size(); }
};

