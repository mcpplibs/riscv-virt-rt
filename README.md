# riscv-virt-rt

Board support for QEMU's RISC-V **`virt`** machine. Depend on it and a bare-metal
C++ project builds, runs and tests with no board knowledge of its own.

```toml
[dependencies]
mcpplibs.riscv-virt-rt = "0.3"

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

## Starting from scratch

```bash
mcpp new blinky --template riscv-virt-rt
cd blinky && mcpp run
```

The template ships **with this package**, so it cannot drift from it: `mcpp new`
writes the dependency line at the version it resolved.

## Without a C library

A project on the zero-libc tier — `[target.riscv64-none-elf].sysroot = ""` — has
no `<stdio.h>`, no startup object and no runtime. That is the arrangement a
kernel or a bootloader begins from, and this board serves it:

```toml
[dependencies]
riscv-virt-rt = { version = "0.5.0", features = ["nolibc"] }
```

⭐ **The C library is a feature of how this board is consumed, not a property of
the board.** Versions 0.4.0 and 0.4.1 assumed the opposite — first refusing the
combination, then degrading silently — and both were wrong for the same reason:
a developer working without a C library needs *more* of a board package, not
less. Where the UART sits, where RAM begins and which emulator boots this
machine are not C library facts, and refusing merely moved them, hardcoded, into
every kernel project.

With the feature the board contributes exactly that half: `virt.ld`, a console
that writes the UART directly, and the emulator. Without it, the request really
is contradictory and is refused with a message naming the feature.

Two templates target this tier, and the difference between them is not what the
program does — it is how much of this board's datasheet is copied into the
project:

```bash
mcpp new k --template riscv-virt-rt:nolibc       # depends on nothing: 5 files
mcpp new k --template riscv-virt-rt:nolibc-bsp   # takes the board's half: 2 files
```

The first is the accurate picture of what a kernel requires, and the better one
to read first. The second is the one to maintain.

What the board does **not** supply on this tier, because supplying it would mean
supplying a second one: formatting (`std-freestanding`), an allocator
(`std-freestanding` with `alloc-kal`), and `memcpy`/`memset`
(`std-freestanding-nolibc`, which provides them once for every board).

## Adding it to a project

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

- **mcpp ≥ 2026.8.19.4**, which is where the target began supplying its own C library. Two earlier boundaries, worth keeping apart:

  - **2026.8.19.2** is the hard floor: `build.mcpp` calls `mcpp::runner`, which
    arrived there.
  - **2026.8.19.3** is the version to actually use. On 2026.8.19.2 this package
    builds and `mcpp run` works, but `mcpp build` **followed by** `mcpp run`
    executes the firmware on the host instead of the emulator — it prints
    nothing and exits 1.

  Below the hard floor the build stops while compiling `build.mcpp`:

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
- `xim:qemu-riscv`, declared in `[xlings].deps` and located through
  `mcpp::xpkg_dir`.

  ⚠️ The target's **C library is not declared here** and should not be. It is a
  property of the target, and mcpp resolves it from the target's own row the
  same way it resolves the compiler — its headers are on every compile line and
  its directory on the link search path before this package says anything. That
  is why the linker line below is bare names (`-lc`, `-lcrt0-semihost`): this
  package chooses *which* pieces, not *where* they are.

## Why the libc headers are not yours

`build.mcpp` emits them as a **package-private** include directory, and mcpp
keeps them that way on purpose. They are `riscv*-none-elf` headers; a consumer
that could `#include <stdio.h>` would be getting a header for a target its own
translation unit may not be built for. What crosses the boundary is the module.

## License

Apache-2.0. The payloads it links carry their own: picolibc is BSD-3-Clause /
BSD-2-Clause, compiler-rt is Apache-2.0 WITH LLVM-exception.
