module;
// Private to this package — see the include_dir note in build.mcpp. A consumer
// never sees these headers; it sees what this file exports.
// ⚠️ The C headers, and therefore the GLOBAL names. `::printf` lives in
// <cstdio>, which is a libc++ header — and libc++ is not available here (its
// __config_site is generated for the host). This package is the boundary
// between the target's C library and a consumer's C++ module, so the
// unqualified names stop here and never reach the consumer.
//
// ⚠️ AND THEREFORE CONDITIONAL. On the zero-libc tier there is no <stdio.h> to
// include, and an unconditional include is what made this package fail with
// `fatal error: 'stdio.h' file not found` — a path inside a package the reader
// did not write, for a reason that appeared nowhere in the message.
#ifndef MCPP_FEATURE_NOLIBC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
export module mcpplibs.riscv_virt_rt;

// ⚠️ WHY THIS PACKAGE HAS A `nolibc` FEATURE AT ALL, WHEN IT IS A PICOLIBC
// BOARD.
//
// The first answer to "no C library" was to refuse: this board is written
// against picolibc, so a project that declines the C library and still depends
// on this board was making two statements that cannot both hold. That answer
// was wrong, and the question that exposed it is worth writing down — does a
// RISC-V developer working without a C library need a board package?
//
// They need MORE of one, not less. Which emulator boots this machine, where
// its UART is, where its power-off register is, where RAM begins — none of that
// is a C library fact, and all of it is what a board package exists to know.
// Refusing simply moved that knowledge into every kernel project, hardcoded,
// which is the coupling a BSP is for.
//
// So the C library is a FEATURE of how this board is consumed, not a property
// of the board. What follows below is the same machine described twice: once
// through picolibc, and once directly.
export namespace board {

#ifdef MCPP_FEATURE_NOLIBC

// ── Console, with no C library beneath it ──────────────────────────────────
//
// `virt`'s 16550A UART. Writing the transmit-holding register is the whole of
// it: the emulated device is always ready, so there is no status register to
// poll, and adding one would be describing hardware this machine does not have.
//
// ⚠️ This is a DIFFERENT device from the one the picolibc path prints through.
// picolibc's stdout goes to the semihosting channel, which is the debugger's,
// and reaches the terminal because `-semihosting` is on the emulator's command
// line. The UART reaches it because `-nographic` puts it there. Both work; they
// are not the same path, and a program that mixes them will see its output
// interleave.
inline void putc(char c) {
    *reinterpret_cast<volatile unsigned char*>(0x10000000) = static_cast<unsigned char>(c);
}
inline void print(const char* s)   { for (; s && *s; ++s) putc(*s); }
inline void println(const char* s) { print(s); putc('\n'); }

// ⚠️ WHAT IS DELIBERATELY ABSENT ON THIS PATH, AND WHERE IT COMES FROM INSTEAD.
//
//   printf   — formatting is the C library's work, and a board package that
//              grew its own would be a small, worse one. `std-freestanding`
//              carries <format>; on the zero-libc tier it needs that package's
//              `nolibc` feature, which supplies the four C headers libc++'s
//              wrappers reach for.
//
//   alloc    — an allocator is a whole-program singleton, so it is chosen by
//   release    the program rather than supplied by a board. `std-freestanding`
//              with `features = ["alloc-kal"]` takes one from whichever openkal
//              implementation is present; `openkal-opensbi` provides one that
//              needs no C library.
//
//   copy     — the compiler is entitled to emit calls to memcpy and memset from
//   fill       code that names neither, so these must exist beneath any C++ on
//              this tier. That makes them the C environment's, not the board's:
//              `std-freestanding-nolibc` provides them, once, for every board.
//
// Each of those is a package a project adds when it wants the thing. A board
// package that supplied its own would be supplying a second one.

#else

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

#endif

// ── Machine ────────────────────────────────────────────────────────────────
//
// `virt`'s syscon. Ending a run on the firmware's terms rather than on the
// emulator's timeout is what makes `mcpp test` able to report a verdict at
// all — picolibc's exit() reaches the same place through semihosting, so
// returning from main is the ordinary way to finish.
//
// ⚠️ OUTSIDE THE CONDITIONAL, AND IT WAS ALREADY WRITTEN THIS WAY BEFORE THERE
// WAS A CONDITIONAL. A store to a fixed address needs no C library, and the
// fact that this one function was always libc-free is the shortest argument
// that the rest did not have to be either.
[[noreturn]] inline void poweroff() {
    *reinterpret_cast<volatile unsigned int*>(0x100000) = 0x5555;
    for (;;) { }
}

}  // namespace board
