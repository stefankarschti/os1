ENTRY(_start)

KERNEL_SHIM_VIRTUAL_BASE = 0xFFFFFFFF90000000;

SECTIONS
{
  .text KERNEL_SHIM_VIRTUAL_BASE :
  {
    code = .; _code = .; __code = .;
    *(.text*)
    . = ALIGN(16);
    KEEP(*(.limine_requests_start))
    KEEP(*(.limine_requests))
    KEEP(*(.limine_requests_end))
    . = ALIGN(4096);
  }

  .data :
  {
     data = .; _data = .; __data = .;
     *(.data*)
     *(.rodata*)
     . = ALIGN(4096);
  }

  .bss :
  {
    bss = .; _bss = .; __bss = .;
    *(.bss*)
    . = ALIGN(4096);
  }

  end = .; _end = .; __end = .;
}
