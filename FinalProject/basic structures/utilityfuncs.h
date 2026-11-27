#pragma once
#include <string>
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