// tracker.h - allocation tracker (Section 5.12).
// Every allocation in the program (including string's) goes through the
// global operator new / delete replaced here, so the exit report can detect leaks.
#pragma once
#include <cstdlib>
#include <cstddef>
using namespace std;
#include <new>

namespace trk {
unsigned long made = 0;
unsigned long freed = 0;
}

void* operator new(size_t n) {
    void* p = malloc(n ? n : 1);
    if (!p) throw bad_alloc();
    trk::made++;
    return p;
}
void* operator new[](size_t n) { return operator new(n); }
void* operator new(size_t n, const nothrow_t&) noexcept {
    void* p = malloc(n ? n : 1);
    if (p) trk::made++;
    return p;
}
void* operator new[](size_t n, const nothrow_t&) noexcept {
    return operator new(n, nothrow);
}
void operator delete(void* p) noexcept {
    if (p) { trk::freed++; free(p); }
}
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void operator delete[](void* p, size_t) noexcept { operator delete(p); }
void operator delete(void* p, const nothrow_t&) noexcept { operator delete(p); }
void operator delete[](void* p, const nothrow_t&) noexcept { operator delete(p); }
