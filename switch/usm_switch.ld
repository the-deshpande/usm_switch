/* usm_switch.ld -- linker script for the USM Switch port.
 *
 * A copy of devkitPro's libnx switch.ld (from the devkitPro install in use
 * at the time of writing: devkitA64 gcc 16.1.0, November 2026), with ONE
 * change: the .rodata of libstdc++.a, libg.a and libsysbase.a is routed
 * into .data.rel.ro instead of .rodata (see below). If devkitPro updates
 * switch.ld, re-copy it and re-apply that one change; everything else in
 * this file should stay identical to upstream.
 *
 * Why: those archives carry C++ typeinfo/vtable data (libstdc++ guard.o,
 * bad_alloc.o, stdexcept.o, class_type_info.o, si_class_type_info.o,
 * tinfo.o, ...) and newlib's _C_time_locale table (libg.a timelocal.o)
 * whose absolute pointers need R_AARCH64_RELATIVE relocations. Left in
 * .rodata they produce a DT_TEXTREL: 162 RELR targets in .rodata in the
 * 2026-11-22 build. The platform link (switch.specs) uses -z text, so
 * that is a link error by default; allowing it with -z,notext instead
 * produces an NRO that crashes at startup in libnx __nx_dynamic's RELR
 * loop, which writes the relocations into the read-only .rodata segment
 * (Atmosphere 2168-0002, Data Abort in _dynProcessRelr, first target
 * libstdc++ guard.o typeinfo at .rodata+0x18dd4d0; the working upstream
 * build has no DT_TEXTREL). Moving those input sections to .data.rel.ro
 * (the RW RELRO segment, where the other 105k relative relocations
 * already live) removes the TEXTREL entirely.
 */
OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS
{
	code   PT_LOAD FLAGS(5) /* Read | Execute */;
	rodata PT_LOAD FLAGS(4) /* Read */;
	data   PT_LOAD FLAGS(6) /* Read | Write */;
	dyn    PT_DYNAMIC;
}

SECTIONS
{
	PROVIDE_HIDDEN( __start__ = 0x0 );

	/* =========== CODE section =========== */
	. = __start__;
	PROVIDE_HIDDEN( __code_start = . );

	.text :
	{
		KEEP (*(.crt0))
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(.text .stub .text.* .gnu.linkonce.t.*)
		. = ALIGN(8);
	} :code

	.init :
	{
		KEEP( *(.init) )
		. = ALIGN(8);
	} :code

	.plt :
	{
		*(.plt)
		*(.iplt)
		. = ALIGN(8);
	} :code

	.fini :
	{
		KEEP( *(.fini) )
		. = ALIGN(8);
	} :code

	/* =========== RODATA section =========== */
	. = ALIGN(0x1000);
	PROVIDE_HIDDEN( __rodata_start = . );

	.nx-module-name : { KEEP (*(.nx-module-name)) } :rodata

	.rela.dyn : { *(.rela.*) } :rodata
	.relr.dyn : { *(.relr.*) } :rodata

	.hash     : { *(.hash) } :rodata
	.gnu.hash : { *(.gnu.hash) } :rodata

	.dynsym   : { *(.dynsym) } :rodata
	.dynstr   : { *(.dynstr) } :rodata

	.rodata :
	{
		*(EXCLUDE_FILE (*libstdc++.a *libg.a *libsysbase.a) .rodata .rodata.* .gnu.linkonce.r.*)
		. = ALIGN(8);
	} :rodata

	.tls.align :
	{
		QUAD( MAX( ALIGNOF(.tdata), ALIGNOF(.tbss) ) )
	} :rodata

	PROVIDE_HIDDEN( __tls_align = ADDR(.tls.align) );

	.gcc_except_table  : { *(.gcc_except_table .gcc_except_table.*) } :rodata
	.eh_frame_hdr      : { *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*) } :rodata
	.eh_frame          : { KEEP (*(.eh_frame)) *(.eh_frame.*) } :rodata
	.gnu_extab         : { *(.gnu_extab*) } : rodata
	.exception_ranges  : { *(.exception_ranges .exception_ranges*) } :rodata

	PROVIDE_HIDDEN( __eh_frame_hdr_start = ADDR(.eh_frame_hdr)                         );
	PROVIDE_HIDDEN( __eh_frame_hdr_end   = ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr) );

	.gnu.version       : { *(.gnu.version) } :rodata
	.gnu.version_d     : { *(.gnu.version_d) } :rodata
	.gnu.version_r     : { *(.gnu.version_r) } :rodata

	.note.gnu.build-id : { *(.note.gnu.build-id) } :rodata

	/* =========== RELRO section =========== */
	. = ALIGN(0x1000);
	PROVIDE_HIDDEN( __relro_start = . );

	.preinit_array :
	{
		PROVIDE_HIDDEN( __preinit_array_start = . );
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN( __preinit_array_end = . );
	} :data

	.init_array :
	{
		PROVIDE_HIDDEN( __init_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)) )
		KEEP( *(.init_array .ctors) )
		PROVIDE_HIDDEN( __init_array_end = . );
	} :data

	.fini_array :
	{
		PROVIDE_HIDDEN( __fini_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)) )
		KEEP( *(.fini_array .dtors) )
		PROVIDE_HIDDEN( __fini_array_end = . );
	} :data

	.data.rel.ro :
	{
		/* .rodata of these archives carries R_AARCH64_RELATIVE relocations
		   (C++ typeinfo/vtables, newlib locale tables); it must live in the
		   writable .data.rel.ro segment, not read-only .rodata (see header). */
		*libstdc++.a:(.rodata .rodata.*)
		*libg.a:(.rodata .rodata.*)
		*libsysbase.a:(.rodata .rodata.*)
		*(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
		*(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)
		. = ALIGN(8);
	} :data

	.dynamic : { *(.dynamic) } :data :dyn

	.got     : { *(.got) *(.igot) } :data
	.got.plt : { *(.got.plt)  *(.igot.plt) } :data

	PROVIDE_HIDDEN( __got_start__ = ADDR(.got)                        );
	PROVIDE_HIDDEN( __got_end__   = ADDR(.got.plt) + SIZEOF(.got.plt) );

	/* =========== DATA section =========== */
	. = ALIGN(0x1000);
	PROVIDE_HIDDEN( __data_start = . );

	.data :
	{
		*(.data .data.* .gnu.linkonce.d.*)
		SORT(CONSTRUCTORS)
		. = ALIGN(8);
	} :data

	.tdata :
	{
		*(.tdata .tdata.* .gnu.linkonce.td.*)
		. = ALIGN(8);
	} :data

	PROVIDE_HIDDEN( __tdata_lma     = ADDR(.tdata)                  );
	PROVIDE_HIDDEN( __tdata_lma_end = ADDR(.tdata) + SIZEOF(.tdata) );

	.tbss :
	{
		*(.tbss .tbss.* .gnu.linkonce.tb.*)
		*(.tcommon)
		. = ALIGN(8);
	} :data

	.bss :
	{
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(8);
	} :data

	/* Reserve space for the TLS segment of the main thread */
	.main.tls ALIGN(MAX(ALIGNOF(.tdata),ALIGNOF(.tbss))) :
	{
		. += SIZEOF(.tdata);
		. = ALIGN(ALIGNOF(.tbss));
		. += SIZEOF(.tbss);
	} :data

	PROVIDE_HIDDEN( __tls_start   = ADDR(.main.tls)                     );
	PROVIDE_HIDDEN( __tls_end     = ADDR(.main.tls) + SIZEOF(.main.tls) );

	PROVIDE_HIDDEN( __bss_start__ = ADDR(.bss) );
	PROVIDE_HIDDEN( __bss_end__   = __tls_end  );

	PROVIDE_HIDDEN( __end__ = . );

	/* =========== Argument buffer =========== */
	. = ALIGN(0x1000);
	PROVIDE_HIDDEN( __argdata__ = . );

	/* ==================
	   ==== Metadata ====
	   ================== */

	/* Discard sections that difficult post-processing */
	/DISCARD/ : { *(.group .comment .note) }

	/* Stabs debugging sections. */
	.stab          0 : { *(.stab) }
	.stabstr       0 : { *(.stabstr) }
	.stab.excl     0 : { *(.stab.excl) }
	.stab.exclstr  0 : { *(.stab.exclstr) }
	.stab.index    0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }

	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0. */

	/* DWARF 1 */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }

	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }

	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }

	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
}
