import mcpplibs.riscv_virt_rt;

// Nothing here names picolibc, compiler-rt, a linker script, a load address,
// -nostdlib or an emulator. That is the point of the package.
extern "C" int main() {
    board::println("hello from qemu virt");
    board::printf("float %.4f\n", 3.14159);

    void* p = board::alloc(64);
    board::println(p ? "heap ok" : "heap FAILED");
    board::release(p);
    return p ? 0 : 1;
}
