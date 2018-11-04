ENTRY(_Z10KernelMainP17SystemInformation)
SECTIONS
{
    .text (0x100000) : AT(0x100000) { *(.text*) } 
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }

   /DISCARD/ : { *(.comment) }
}

