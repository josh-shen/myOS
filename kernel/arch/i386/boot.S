# Declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Declare a header as in the Multiboot Standard.
.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Reserve a stack for the initial thread.
.section .bootstrap_stack, "aw", @nobits
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# Preallocate pages used for paging.	
.section .bss, "aw", @nobits
.align 4096
boot_page_directory:
	.skip 4096
boot_page_table:
	.skip 4096

# The kernel entry point.
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
	# Save multiboot info from ebx.
	movl $(stack_top - 0xC0000000), %esp
	push %ebx
	push %eax

	# Physical address of boot_page_table.
	movl $(boot_page_table - 0xC0000000), %edi
	# First address to map is address 0.
	movl $0, %esi
	# Map 1023 pages. The 1024th will be the VGA text buffer.
	movl $1023, %ecx
1:	
	# Map physical address as "present, writable".
	# TODO: .text and .rodata are mapped as writable. They should be read-only.
	movl %esi, %edx
	orl $0x003, %edx
	movl %edx, (%edi)
	# Size of page is 4096 bytes.
	addl $4096, %esi
	# Size of entries in boot_page_table is 4 bytes.
	addl $4, %edi
	# Loop to the next entry if we haven't finished.
	loop 1b

	# Map the VGA buffer to the last page (1023).
	movl $(0x000B8000 | 0x003), boot_page_table - 0xC0000000 + 1023 * 4

	# Add identity mapping and higher-half mapping of the first 4 MiB to the page directory
	movl $(boot_page_table - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 0
	movl $(boot_page_table - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 768 * 4

	# Load the page directory into CR3.
	movl $(boot_page_directory - 0xC0000000), %ecx
	movl %ecx, %cr3

	# Enable paging and the write-protect bit.
	movl %cr0, %ecx
	orl $0x80010000, %ecx
	movl %ecx, %cr0

	# Jump to higher half with an absolute jump.
	lea 2f, %ecx
	jmp *%ecx
.size _start, . - _start
.section .text
2:
	# Flush the TLB.
	movl %cr3, %ecx
	movl %ecx, %cr3 

	addl $0xC0000000, %esp

	# Call the global constructors.
	call _init

	# Transfer control to the main kernel.
	call kernel_main

	# Hang if kernel_main unexpectedly returns.
	cli
1:	hlt
	jmp 1b

# Global Descriptor Table
.global gdt_flush
gdt_flush:
	# Load GDT
	mov 4(%esp), %eax
	lgdt (%eax)
		
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es 
	mov %ax, %fs
	mov %ax, %gs 
	mov %ax, %ss

	jmp $0x08, $flush
flush:
	ret

# Tnterrupt Descriptor Table
.global idt_flush
idt_flush:
	# Load IDT
	mov 4(%esp), %eax
	lidt (%eax)
	ret

# Tnterrupt Service Routines
.macro isr_err_stub num
.global isr\num
isr\num:
	cli
	push $\num
	jmp isr_common_stub
.endm

.macro isr_no_err_stub num
.global isr\num
isr\num:
	cli
	push $0 # Placeholder error code
	push $\num
	jmp isr_common_stub
.endm

isr_common_stub:
	pusha				# Push eax, ecx, edx, ebx, esp, ebp, esi, edi

	mov %ds, %ax       	# Move current state to ax
    push %eax          	# Push the data segment descriptor
    
    mov $0x10, %ax     
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
	push %esp			# Push handler argument (register struct) to the stack as a pointer
    call isr_handler 	# Otherwise, values in the struct may be corrupted
    add $4, %esp	    # Cleans up the pushed register struct pointer

    pop %eax           	# Reload the original data segment descriptor
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    popa                
    
    add $8, %esp   		# Cleans up the pushed error code and pushed ISR number
    iret  

.extern isr_handler
isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub 8
isr_no_err_stub 9
isr_err_stub 10
isr_err_stub 11
isr_err_stub 12
isr_err_stub 13
isr_err_stub 14
isr_no_err_stub 15
isr_no_err_stub 16
isr_no_err_stub 17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_no_err_stub 30
isr_no_err_stub 31

# Interrupt Request Routines
.macro irq_err_stub num val
.global irq\num
irq\num:
	cli
	push $0
	push $\val
	jmp irq_common_stub
.endm

irq_common_stub:
	pusha

	mov %ds, %ax
	push %eax

	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	push %esp
	call irq_handler
	add $4, %esp

	pop %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	popa

	add $8, %esp
	iret

.extern irq_handler
irq_err_stub 0, 32
irq_err_stub 1, 33
irq_err_stub 2, 34
irq_err_stub 3, 35
irq_err_stub 4, 36
irq_err_stub 5, 37
irq_err_stub 6, 38
irq_err_stub 7, 39
irq_err_stub 8, 40
irq_err_stub 9, 41
irq_err_stub 10, 42
irq_err_stub 11, 43
irq_err_stub 12, 44
irq_err_stub 13, 45
irq_err_stub 14, 46
irq_err_stub 15, 47