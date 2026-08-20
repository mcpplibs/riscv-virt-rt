/* The memory map of QEMU's RISC-V `virt` machine.
 *
 * WHY THIS FILE IS IN THE BOARD PACKAGE AND NOT IN EVERY PROJECT
 *
 * Where this machine begins executing, and where its RAM is, are the two facts
 * a program cannot derive and cannot guess. They are also the two facts that do
 * not change between one program on this board and the next — which makes them
 * exactly what a board package is for. Every kernel project carrying its own
 * copy of this file is the coupling a BSP exists to remove.
 *
 * Emitted by build.mcpp when the `nolibc` feature is active. On the picolibc
 * path the C library supplies its own script (`picolibcpp.ld`), which knows
 * about its startup object and its own sections, and this one is not used.
 *
 * ⚠️ A PROJECT THAT WANTS A DIFFERENT MAP SHOULD NOT TAKE THIS ONE. Two linker
 * scripts describing the same output sections do not compose. The arrangement
 * for a project with its own map is to depend on no board package at all and
 * write the three files itself — a linker script, an entry point and a runner
 * line — which is what `--template riscv-virt-rt:nolibc` produced before this
 * file existed, and remains a supported way to work.
 */
ENTRY(_start)

SECTIONS {
    . = 0x80000000;

    /* .text.entry first, and deliberately: execution begins at the load
     * address, not at whichever function the linker chose to place there. */
    .text : {
        *(.text.entry)
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }

    /* Not zeroed by anything here. A C runtime would have done it before main;
     * a program on this tier has none, so either _start clears it or the
     * program does not rely on it. The symbols are exported so that either
     * choice is available. */
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . = . + 0x4000;          /* 16 KiB of stack */
    __stack_top = .;
}
