// The openkal implementation this board supplies, exercised rather than linked.
//
// ⚠️⚠️ WHY THIS FILE EXISTS: `console.cpp` CALLS `board::*` AND NOTHING ELSE.
//
// The `openkal` feature compiles `src/kal/**` and the linker takes all fourteen
// names into every test binary, so `nm` shows them and the suite passes. That
// proves the implementation COMPILES AND LINKS. It proves nothing about what
// any of it answers -- and when the surface moved to openkal 0.9 (one-word
// transfers instead of a count-and-condition pair, a granularity operation, a
// self-description) a wrong answer in any of them would still have linked and
// still have shown green.
//
// ⭐ EVERY OBSERVATION BELOW IS OF SOMETHING 0.9 INTRODUCED OR CHANGED, so a
// build against an older specification does not reach this file, and a
// regression in the shapes this version adopted fails it.
//
// ⚠️ AND IT DOES NOT GO QUIET WHEN THE FEATURE IS OFF. Without the feature there
// is no implementation to ask, so the file prints that it did not observe
// anything and returns 0 -- and continuous integration greps for the line that
// only appears when it DID, rather than for the suite's exit status. A test
// whose "not run" and whose "passed" produce the same reading is not a test.
#ifdef MCPP_FEATURE_OPENKAL

import openkal.types;
import openkal.version;
import openkal.abort;
import openkal.stream;
import openkal.memory;

namespace {

int failures = 0;

kal_uintptr length(const char* s) { kal_uintptr n = 0; while (s[n]) ++n; return n; }

void say(const char* s) {
    const kal_uintptr n = length(s);
    kal_uintptr done = 0;
    while (done < n) {
        // The count, or a negated condition. A count of zero would loop for
        // ever, so it ends the attempt -- which is the shape 0.9 requires of
        // every caller and is therefore also what this exercises.
        const kal_intptr r = kal_stream_write(kal_stdout(), s + done, n - done);
        if (r <= 0) return;
        done += static_cast<kal_uintptr>(r);
    }
}

void check(bool held, const char* what) {
    say(held ? "ok:   " : "FAIL: ");
    say(what);
    say("\n");
    if (!held) ++failures;
}

}  // namespace

extern "C" int main() {
    say("openkal: the board's own implementation\n");

    // The self-description, which is the only way a consumer that is not linked
    // can ask. Both are exported by every conforming implementation.
    check(kal_version() >= kal::header_version,
          "the implementation is at least as new as the declarations");

    // ⭐ EXACTLY THE THREE, AND NOT MERELY AT LEAST THEM. A word that claimed an
    // interface this board does not export would mislead precisely the consumer
    // with no linker to ask, so the absence is asserted as firmly as the
    // presence.
    const auto claimed = kal_interfaces();
    const auto core    = (kal::iface::abort_ | kal::iface::stream
                                            | kal::iface::memory).bits;
    check(claimed == core, "it claims the core set and nothing beyond it");

    // A transfer is one signed word. Zero bytes is a count of zero and not a
    // condition -- the distinction the old two-word form made by other means.
    check(kal_stream_write(kal_stdout(), "", 0) == 0,
          "a transfer of nothing reports a count of nothing");

    // A stream this implementation does not have is refused, and the refusal is
    // negative rather than a large unsigned count.
    check(kal_stream_write(kal_stream{99}, "x", 1) < 0,
          "a stream that does not exist is refused with a negative value");

    check(kal_stream_flush(kal_stdout()) == kal_ok, "the output stream flushes");

    // Every stream here reaches the host through semihosting, which is
    // interactive in the sense that matters: output must not wait for a buffer.
    // ⚠️ THE MODULE SPELLING, NOT THE MACRO. A macro is not exportable from a
    // module, so `KAL_STREAM_PROP_INTERACTIVE` is not in scope here however
    // plainly it is spelled in the header -- which is the seam clause 4.2 adds
    // `kal::stream_prop::` to remove.
    check((kal_stream_props(kal_stdout()) & kal::stream_prop::interactive.bits) != 0,
          "the console reports itself interactive");

    // ⭐ ONE, AND ONE IS AN ANSWER. This machine has no memory management unit
    // and this implementation imposes no rounding, so every address and every
    // length is acceptable. A consumer must not read it as a page size.
    const kal_uintptr grain = kal_memory_granularity();
    check(grain == 1, "the granularity is one, which is what this machine has");

    // The allocator, at an alignment the default would not give.
    {
        void* p = kal_alloc(256, 64);
        bool held = p != nullptr && (reinterpret_cast<kal_uintptr>(p) % 64) == 0;
        if (p) {
            auto* b = static_cast<unsigned char*>(p);
            for (int i = 0; i < 256; ++i) b[i] = static_cast<unsigned char>(i);
            for (int i = 0; i < 256; ++i)
                if (b[i] != static_cast<unsigned char>(i)) held = false;
            kal_free(p, 256, 64);
        }
        check(held, "memory is obtained at the alignment asked for and holds");
    }

    // openkal.abort is exercised by not being exercised: calling it would end
    // the program, so the observation is that the program chose not to.
    check(true, "the program reached the end without terminating abnormally");

    say(failures ? "openkal: NOT HELD\n" : "openkal: every observation held\n");
    return failures;
}

#else

// No implementation to ask. Say so in a line that is not the line continuous
// integration asserts on.
extern "C" int main() {
    static const char m[] =
        "openkal: not observed --- built without the openkal feature\n";
    // No openkal here by definition, so the C library beneath is what is left.
    for (const char* p = m; *p; ++p) __builtin_putchar(*p);
    return 0;
}

#endif
