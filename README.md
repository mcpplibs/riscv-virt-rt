# riscv-virt-rt

Board support for QEMU's RISC-V **`virt`** machine. Depend on it and a bare-metal
C++ project builds, runs and tests with no board knowledge of its own.

```toml
[dependencies]
mcpplibs.riscv-virt-rt = "0.1"

[targets.firmware]
kind = "bin"
main = "src/main.cpp"
```

```cpp
import mcpplibs.riscv_virt_rt;

extern "C" int main() {
    board::println("hello from qemu virt");
    board::printf("float %.4f\n", 3.14159);
    return 0;
}
```

```bash
mcpp build --target riscv64-none-elf     # firmware + .bin + .map + a size summary
mcpp run   --target riscv64-none-elf     # boots in qemu
mcpp test  --target riscv64-none-elf     # each tests/*.cpp runs as its own image
```

⭐ Nothing in that project names picolibc, compiler-rt, `crt0`, a linker script,
a load address, `-nostdlib`, `-mcmodel` — or an emulator. There is no
`[target.*]` section at all.

## What it supplies

| | |
|---|---|
| C library | picolibc, per ISA profile, privately included and re-exported as a module |
| Runtime | compiler-rt builtins ⚠️ **not optional**: picolibc's `printf` formats floats through ryu, which does 128-bit shifts, and rv64 has no instruction for them |
| Startup | picolibc's semihosting `crt0`, so an ordinary `int main()` works |
| Memory layout | picolibc's linker script for the `virt` map |
| Runner | `qemu-system-riscv{32,64}` by **absolute path**, with the machine model and firmware mode |

## Targets

| `--target` | ISA profile |
|---|---|
| `riscv64-none-elf` | `rv64gc` / `lp64d` |
| `riscv32-none-elf` | `rv32imac` / `ilp32` |

One package serves both: the profile is chosen from the target mcpp resolved,
not from a second copy of this file.

## Requirements

- **mcpp ≥ 2026.8.19.2**, because `build.mcpp` calls `mcpp::runner`, which
  arrived in that version. On an older mcpp the build stops while compiling
  `build.mcpp`:

  ```
  error: 'runner' is not a member of 'mcpp'
         The `mcpp` build module this engine bundles does not have that name.
         Either the package was written for a newer mcpp (try `mcpp self update`…
  ```

  ⚠️ The floor is stated here in prose because a package **cannot** probe for
  it. `if constexpr (requires { mcpp::runner("x"); })` is a hard error when the
  name is absent, not `false` — a requires-expression over a qualified name
  that does not exist is ill-formed. There is no in-language feature test, so
  the version is documented and the diagnostic above is the fallback.
- `xim:picolibc-riscv` and `xim:qemu-riscv`. Declared in `[xlings].deps` here
  and installed with this package, and located at build time through
  `mcpp::xpkg_dir` rather than by reconstructing a store path.

## Why the libc headers are not yours

`build.mcpp` emits them as a **package-private** include directory, and mcpp
keeps them that way on purpose. They are `riscv*-none-elf` headers; a consumer
that could `#include <stdio.h>` would be getting a header for a target its own
translation unit may not be built for. What crosses the boundary is the module.

## License

Apache-2.0. The payloads it links carry their own: picolibc is BSD-3-Clause /
BSD-2-Clause, compiler-rt is Apache-2.0 WITH LLVM-exception.
