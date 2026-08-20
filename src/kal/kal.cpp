// openkal's core interfaces, implemented for QEMU's RISC-V `virt` machine.
//
// WHY THE BOARD PACKAGE SUPPLIES THIS, AND NOT A PACKAGE OF ITS OWN
//
// openkal's design sketched a bare-metal backend selected by an ISA-level
// predicate:
//
//     [target.'cfg(all(arch = "riscv64", os = "none"))'.dependencies]
//     openkal-uart = "0.1"
//
// That key is wrong, and wrong in the quiet direction. The backend's console
// is a store to a fixed address, and which address is a BOARD fact, not an ISA
// fact. On a second RISC-V board the same predicate still matches, the same
// package is still selected, and the program writes to an address that is not
// a UART: it compiles, links, runs, and prints nothing.
//
// The correct selection key is the board dependency that a bare-metal project
// already has. This file therefore lives with the board, behind a feature, and
// a consumer that wants openkal writes `features = ["openkal"]` and no `cfg`
// at all.
//
// WHAT IS IMPLEMENTED, AND WHY NOT MORE
//
// abort, stream and memory — openkal's core set. An implementation provides an
// interface in whole or not at all, so the absence of `fs`, `process`, `task`,
// `env` and `time` is not a deviation: `import openkal.task;` simply does not
// resolve, which is the diagnostic a bare-metal author wants.
//
// ⚠️ EVERYTHING IS BUILT ON THE C LIBRARY BENEATH IT, NOT BESIDE IT.
//
// openkal's own specification states the rule for allocation: where the
// environment already provides an allocator, an implementation is built upon
// it and not beside it, because two allocators drawing on one region of memory
// is a defect that appears only under load. The same reasoning applies to the
// console — picolibc's stdout is already wired to semihosting, and a second
// path to the same device would interleave.

#include <stdio.h>
#include <stdlib.h>
#include <openkal/abort.h>
#include <openkal/memory.h>
#include <openkal/stream.h>

namespace {
// Handles are opaque to the caller. Small indices suffice here because this
// implementation has exactly three streams and no way to make more.
constexpr kal_uintptr kStdin  = 0;
constexpr kal_uintptr kStdout = 1;
constexpr kal_uintptr kStderr = 2;
}  // namespace

extern "C" {

// ── openkal.abort ───────────────────────────────────────────────────────────
//
// `_Exit` and not `exit`: openkal specifies that no destructor and no atexit
// handler runs, and on this board the status reaches the host through
// semihosting either way.
//
// ⚠️ `kal_abort` carries a message and its length rather than reading a
// null-terminated string: an aborting program may hold a pointer into a buffer
// that is not terminated, and requiring termination is how a diagnostic path
// becomes the second fault. Written to stderr with no allocation and no
// formatting, because neither can be assumed to work at this point.
KAL_NORETURN void kal_abort(const char* msg, kal_uintptr len) {
    if (msg && len) fwrite(msg, 1, len, stderr);
    fputc('\n', stderr);
    abort();
}
KAL_NORETURN void kal_exit(int code) { _Exit(code); }

// ── openkal.stream ──────────────────────────────────────────────────────────
kal_stream kal_stdin (void) { return kal_stream{kStdin};  }
kal_stream kal_stdout(void) { return kal_stream{kStdout}; }
kal_stream kal_stderr(void) { return kal_stream{kStderr}; }

kal_io_result kal_stream_write(kal_stream s, const void* buf, kal_uintptr n) {
    FILE* f = s.h == kStdout ? stdout : s.h == kStderr ? stderr : nullptr;
    if (!f) return kal_io_result{0, kal_err_invalid};
    // ⚠️ Written in full or reported, never short. openkal moved the retry
    // loop into the implementation precisely so that every caller would not
    // have to write one; POSIX's convention of returning a short count is what
    // makes that necessary elsewhere.
    const auto* p = static_cast<const unsigned char*>(buf);
    kal_uintptr done = 0;
    while (done < n) {
        const auto wrote = fwrite(p + done, 1, n - done, f);
        if (wrote == 0) return kal_io_result{done, kal_err_io};
        done += wrote;
    }
    return kal_io_result{done, kal_ok};
}

kal_io_result kal_stream_read(kal_stream s, void* buf, kal_uintptr n) {
    if (s.h != kStdin) return kal_io_result{0, kal_err_invalid};
    const auto got = fread(buf, 1, n, stdin);
    // End of input is a zero-length success, not an error: semihosting reports
    // it the same way a closed pipe does.
    return kal_io_result{static_cast<kal_uintptr>(got), kal_ok};
}

int kal_stream_flush(kal_stream s) {
    FILE* f = s.h == kStdout ? stdout : s.h == kStderr ? stderr : nullptr;
    if (!f) return kal_err_invalid;
    return fflush(f) == 0 ? kal_ok : kal_err_io;
}

// ⚠️ Required, not optional. An implementation provides an interface in whole
// or not at all, and `kal_stream_props` is in the stream group; omitting it
// would make this a partial implementation of an interface rather than the
// absence of one, which is the distinction the specification is built on. The
// conformance check reads the exported symbol set, so the omission would be
// caught — but the reason to have it is that a C library placed above openkal
// needs the answer before it transfers anything, in order to choose a
// buffering discipline.
//
// All three streams here reach the host through semihosting, which is a
// debugger channel and therefore interactive in the sense that matters: output
// must not wait for a buffer to fill.
kal_uintptr kal_stream_props(kal_stream s) {
    if (s.h != kStdin && s.h != kStdout && s.h != kStderr) return 0;
    return KAL_STREAM_PROP_INTERACTIVE;
}

// ── openkal.memory ──────────────────────────────────────────────────────────
//
// Above picolibc's allocator rather than beside it, per the rule quoted at the
// top of this file. picolibc's `vfprintf` already references `free`, so a
// second allocator over the same region would be reachable from within the
// C library itself.
void* kal_alloc(kal_uintptr size, kal_uintptr align) {
    if (size == 0) size = 1;
    // `__BIGGEST_ALIGNMENT__` rather than `alignof(max_align_t)`: the latter
    // needs <cstddef>, and this translation unit deliberately includes only C
    // headers so that it can be compiled for a target whose C++ library is
    // the freestanding subset rather than a hosted one.
    if (align <= __BIGGEST_ALIGNMENT__) return malloc(size);
    // C11 requires the size to be a multiple of the alignment.
    const kal_uintptr rounded = (size + align - 1) / align * align;
    return aligned_alloc(align, rounded);
}

void kal_free(void* p, kal_uintptr, kal_uintptr) { free(p); }

}  // extern "C"
