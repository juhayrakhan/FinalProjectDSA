// bloom.h - plain Bloom filter (Sections 5.7, 5.10) and counting Bloom filter (Section 5.11).
// Positions use double hashing: position(i) = (h1 + i*h2) mod m, h1 = FNV-1a, h2 = djb2 | 1.
#pragma once
#include "dynamicarray.h"

struct Bloom {
    unsigned int m, k;
    unsigned char* bits;
    Bloom(unsigned int mm, unsigned int kk) : m(mm), k(kk) {
        int nb = ((m + 7) / 8);
        bits = new unsigned char[nb];
        for (int i = 0; i < nb; i++) bits[i] = 0;
    }
    ~Bloom() { delete[] bits; }
    void add(const string& t) {
        unsigned int h1 = fnv1a(t), h2 = djb2(t) | 1u;
        for (unsigned int i = 0; i < k; i++) {
            unsigned long long p = ((unsigned long long)h1 + (unsigned long long)i * h2) % m;
            bits[p >> 3] = (unsigned char)(bits[p >> 3] | (1u << (p & 7)));
        }
    }
    bool test(const string& t) const {          // O(k)
        unsigned int h1 = fnv1a(t), h2 = djb2(t) | 1u;
        for (unsigned int i = 0; i < k; i++) {
            unsigned long long p = ((unsigned long long)h1 + (unsigned long long)i * h2) % m;
            if (!(bits[p >> 3] & (1u << (p & 7)))) return false;
        }
        return true;
    }
    void orWith(const Bloom& o) {
        int nb = ((m + 7) / 8);
        for (int i = 0; i < nb; i++) bits[i] = (unsigned char)(bits[i] | o.bits[i]);
    }
    void copyFrom(const Bloom& o) {
        int nb = ((m + 7) / 8);
        for (int i = 0; i < nb; i++) bits[i] = o.bits[i];
    }
};

struct CountingBloom {
    unsigned int m, k;
    unsigned char* c;                        // counters 0..15
    CountingBloom(unsigned int mm, unsigned int kk) : m(mm), k(kk) {
        c = new unsigned char[m];
        for (unsigned int i = 0; i < m; i++) c[i] = 0;
    }
    ~CountingBloom() { delete[] c; }
    unsigned long long pos(const string& t, unsigned int i) const {
        return ((unsigned long long)fnv1a(t) + (unsigned long long)i * (unsigned long long)(djb2(t) | 1u)) % m;
    }
    void inc(const string& t) {                 // saturates at 15
        for (unsigned int i = 0; i < k; i++) { unsigned long long p = pos(t, i); if (c[p] < 15) c[p]++; }
    }
    void dec(const string& t) {                 // a counter at 15 is never decremented
        for (unsigned int i = 0; i < k; i++) { unsigned long long p = pos(t, i); if (c[p] > 0 && c[p] < 15) c[p]--; }
    }
    bool allPositive(const string& t) const {
        for (unsigned int i = 0; i < k; i++) if (c[pos(t, i)] == 0) return false;
        return true;
    }
    int saturated() const {
        int n = 0;
        for (unsigned int i = 0; i < m; i++) if (c[i] == 15) n++;
        return n;
    }
};
