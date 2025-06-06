/* The bootloader will look at this image and start execution at the symbol
   designated at the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB */
	/* Sections are page aligned */
	. = 1M;

	kernel_start = .;

	/* Multiboot header */
	.multiboot.data : {
		*(.multiboot.data)
	}

	.multiboot.text : {
		*(.multiboot.text)
	}

	/* The kernel will live in the 3 GiB + 1 MiB virtual address space */
	. += 0xC0000000;

	.text ALIGN(4K) : AT (ADDR(.text) - 0xC0000000)
	{
		*(.text)
	}

	/* Read-only data. */
	.rodata ALIGN(4K) : AT (ADDR(.rodata) - 0xC0000000)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN(4K) : AT (ADDR(.data) - 0xC0000000)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K) : AT (ADDR(.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}

	/* PMM bitmap array */
	.pmm_bitmap ALIGN(4k) : AT (ADDR(.pmm_bitmap) - 0xC0000000)
	{
		pmm_bitmap_start = .;
		*(.pmm_bitmap_array)
	}
	
	kernel_end = .;
	/* The compiler may produce other sections, put them in the proper place in
	   in this file, if you'd like to include them in the final kernel. */
}
