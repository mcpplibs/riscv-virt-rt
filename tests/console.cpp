import mcpplibs.riscv_virt_rt;

// A bare-metal test is an ordinary mcpp test: it returns non-zero to fail.
// Semihosting carries the firmware's return value out to the emulator's exit
// code, so `mcpp test --target riscv64-none-elf` reads it the same way it
// reads a host test's.
extern "C" int main() {
    board::println("console works");
    void* p = board::alloc(128);
    if (!p) return 1;
    board::fill(p, 0xA5, 128);
    const auto* b = static_cast<const unsigned char*>(p);
    const bool ok = b[0] == 0xA5 && b[127] == 0xA5;
    board::release(p);
    return ok ? 0 : 1;
}
