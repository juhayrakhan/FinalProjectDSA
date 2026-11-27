#pragma once
#include "dynamicarray.h"

// ---------------------------------------------------------------- Stable merge sort
// less(x, y) must mean "x strictly before y". Equal items keep their order.
template <class X, class C>
void msortRec(X* arr, X* x, int lo, int hi, C lf) {
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

template <class X, class C>
void stableSort(DynamicArray<X>& v, C lf) {
	if (v.size() < 2) return;
	X* tmp = new X[v.size()];
	msortRec(v.arr, tmp, 0, v.size(), lf);
	delete[] tmp;
}

