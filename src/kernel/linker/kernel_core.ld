/* Link.ld -- Linker script for the shared kernel core. */

ENTRY(kernel_main)

KERNEL_PHYSICAL_BASE = 0x00100000;
KERNEL_VIRTUAL_OFFSET = 0xFFFFFFFF80000000;
KERNEL_VIRTUAL_BASE = KERNEL_PHYSICAL_BASE + KERNEL_VIRTUAL_OFFSET;

SECTIONS
{
  . = KERNEL_VIRTUAL_BASE;
  __kernel_virtual_start = .;

  .text : AT(ADDR(.text) - KERNEL_VIRTUAL_OFFSET)
  {
    code = .; _code = .; __code = .;
    __kernel_text_start = .;
    *(.text*)
    . = ALIGN(4096);
    __kernel_text_end = .;
  }

  .rodata : AT(ADDR(.rodata) - KERNEL_VIRTUAL_OFFSET)
  {
    __kernel_rodata_start = .;
    *(.rodata*)
    . = ALIGN(4096);
    __kernel_rodata_end = .;
  }

  .data : AT(ADDR(.data) - KERNEL_VIRTUAL_OFFSET)
  {
    data = .; _data = .; __data = .;
    __kernel_data_start = .;
    *(.data*)
    . = ALIGN(4096);
    __kernel_data_end = .;
  }

  .bss : AT(ADDR(.bss) - KERNEL_VIRTUAL_OFFSET)
  {
    bss = .; _bss = .; __bss = .;
    __kernel_bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4096);
    __kernel_bss_end = .;
  }

  end = .; _end = .; __end = .;
  __kernel_virtual_end = .;
  __kernel_physical_start = LOADADDR(.text);
  __kernel_physical_end = LOADADDR(.bss) + SIZEOF(.bss);
  __kernel_load_start = LOADADDR(.text);
  __kernel_load_end = LOADADDR(.bss) + SIZEOF(.bss);
}