#pragma once
#include "../basic structures/dynamicarray.h"
#include <string>
using namespace std;

// ---------------------------------------------------------------- Text helpers
// token: maximal run of English letters/digits, lowercased (Section 2)
void tokenize(const string& s, DynamicArray<string>& out) {
	out.clear();
	int i = 0, count = s.size();
	while (i < count) {
		if (isCharacter(s[i])) {
			string x;
			while (i < count && isCharacter(s[i])) { x += toLowercase(s[i]); i++; }
			out.push(x);
		}
		else i++;
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
	for (int i = 0; i < len(s); i++) { if (!isDigit(s[i])) return false; v = v * 10 + (s[i] - '0'); }
	out = v;
	return true;
}