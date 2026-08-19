module;
// Private to this package — see the include_dir note in build.mcpp. A consumer
// never sees these headers; it sees what this file exports.
// ⚠️ The C headers, and therefore the GLOBAL names. `::printf` lives in
// <cstdio>, which is a libc++ header — and libc++ is not available here (its
// __config_site is generated for the host). This package is the boundary
// between the target's C library and a consumer's C++ module, so the
// unqualified names stop here and never reach the consumer.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
export module mcpplibs.riscv_virt_rt;

export namespace board {

// ── Console ────────────────────────────────────────────────────────────────
//
// Through the target libc rather than by poking the UART directly: picolibc's
// stdout is already wired to semihosting, so the same code prints under the
// emulator and, with a different picolibc backend, on real hardware.
inline void print(const char* s)                 { fputs(s, stdout); }
inline void println(const char* s)               { fputs(s, stdout); fputc('\n', stdout); }

template <class... Args>
inline int printf(const char* fmt, Args... args) { return ::printf(fmt, args...); }

// ── Heap ───────────────────────────────────────────────────────────────────
inline void* alloc(unsigned long n)                { return malloc(n); }
inline void  release(void* p)                    { free(p); }

// ── Bytes ──────────────────────────────────────────────────────────────────
inline void* copy(void* d, const void* s, unsigned long n) { return memcpy(d, s, n); }
inline void* fill(void* d, int c, unsigned long n)         { return memset(d, c, n); }

// ── Machine ────────────────────────────────────────────────────────────────
//
// `virt`'s syscon. Ending a run on the firmware's terms rather than on the
// emulator's timeout is what makes `mcpp test` able to report a verdict at
// all — picolibc's exit() reaches the same place through semihosting, so
// returning from main is the ordinary way to finish.
[[noreturn]] inline void poweroff() {
    *reinterpret_cast<volatile unsigned int*>(0x100000) = 0x5555;
    for (;;) { }
}

}  // namespace board
