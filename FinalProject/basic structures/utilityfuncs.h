#pragma once
#include <string>
using namespace std;

int len(const string& s) { return s.size(); }
bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isCharacter(char c) { return isLetter(c) || isDigit(c); }
char toLowercase(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
string toLowercase(const string& s) {
    string r = s;
    for (int i = 0; i < r.size(); i++) r[i] = toLowercase(r[i]);
    return r;
}
int getMax(int a, int b) { return a > b ? a : b; }