/* SPDX-License-Identifier: MIT */
/* Tanmatsu Plugin SDK - Linker Script */
/*
 * This linker script defines the memory layout for Tanmatsu plugins.
 * Plugins are loaded into PSRAM at runtime by the kbelf loader.
 */

OUTPUT_FORMAT("elf32-littleriscv")
OUTPUT_ARCH(riscv)

/* Entry point - not used directly but needed for ELF format */
ENTRY(_plugin_entry)

SECTIONS
{
    /* Plugin info section - MUST be first for discovery */
    .plugin_info : ALIGN(4)
    {
        KEEP(*(.plugin_info))
        KEEP(*(.plugin_info.*))
    }

    /* Dynamic linking sections */
    .dynamic : ALIGN(4)
    {
        *(.dynamic)
    }

    .dynsym : ALIGN(4)
    {
        *(.dynsym)
    }

    .dynstr : ALIGN(4)
    {
        *(.dynstr)
    }

    .hash : ALIGN(4)
    {
        *(.hash)
    }

    .gnu.hash : ALIGN(4)
    {
        *(.gnu.hash)
    }

    /* Relocation sections */
    .rela.dyn : ALIGN(4)
    {
        *(.rela.init)
        *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
        *(.rela.fini)
        *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
        *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
        *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
        *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
        *(.rela.ctors)
        *(.rela.dtors)
        *(.rela.got)
        *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
        *(.rela.plt)
        *(.rela*)
    }

    .rela.plt : ALIGN(4)
    {
        *(.rela.plt)
    }

    /* PLT and GOT */
    .plt : ALIGN(4)
    {
        *(.plt)
        *(.plt.*)
    }

    .got : ALIGN(4)
    {
        *(.got)
    }

    .got.plt : ALIGN(4)
    {
        *(.got.plt)
    }

    /* Code section */
    .text : ALIGN(4)
    {
        *(.text.unlikely .text.*_unlikely .text.unlikely.*)
        *(.text.exit .text.exit.*)
        *(.text.startup .text.startup.*)
        *(.text.hot .text.hot.*)
        *(.text .text.* .gnu.linkonce.t.*)
        *(.stub)
    }

    /* Read-only data */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.rodata1)
    }

    /* Exception handling (minimal) */
    .eh_frame_hdr : ALIGN(4)
    {
        *(.eh_frame_hdr)
    }

    .eh_frame : ALIGN(4)
    {
        KEEP(*(.eh_frame))
    }

    /* Initialization arrays */
    .preinit_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    }

    .init_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .fini_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    }

    /* Constructors/Destructors */
    .ctors : ALIGN(4)
    {
        KEEP(*crtbegin*.o(.ctors))
        KEEP(*(EXCLUDE_FILE(*crtend*.o) .ctors))
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
    }

    .dtors : ALIGN(4)
    {
        KEEP(*crtbegin*.o(.dtors))
        KEEP(*(EXCLUDE_FILE(*crtend*.o) .dtors))
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
    }

    /* Data section */
    .data : ALIGN(4)
    {
        *(.data .data.* .gnu.linkonce.d.*)
        *(.data1)
    }

    /* Small data (for GP-relative addressing) */
    .sdata : ALIGN(4)
    {
        *(.sdata .sdata.* .gnu.linkonce.s.*)
    }

    /* BSS - uninitialized data */
    .sbss : ALIGN(4)
    {
        *(.dynsbss)
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon)
    }

    .bss : ALIGN(4)
    {
        *(.dynbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
    }

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.debug*)
    }
}
