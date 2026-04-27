ENTRY(_start)
SECTIONS
{
  .text 0xFFFFFFFF80000000 :
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
