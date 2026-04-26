set(KERNEL_BIOS_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker2.ld")
set(KERNEL_LIMINE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker_limine.ld")
set(KERNEL_ARCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64")

set(KERNEL_CXX_SOURCES
  arch/x86_64/cpu.cpp
  arch/x86_64/interrupt.cpp
  arch/x86_64/ioapic.cpp
  arch/x86_64/lapic.cpp
  arch/x86_64/mp.cpp
  arch/x86_64/pic.cpp
  bootinfo.cpp
  console_input.cpp
  debug.cpp
  display.cpp
  drivers/block/virtio_blk.cpp
  fs/initrd.cpp
  kernel.cpp
  keyboard.cpp
  mm/user_copy.cpp
  pageframe.cpp
  platform.cpp
  platform/acpi.cpp
  platform/pci.cpp
  proc/user_program.cpp
  string.cpp
  syscall/console_read.cpp
  syscall/observe.cpp
  syscall/process.cpp
  syscall/wait.cpp
  task.cpp
  terminal.cpp
  virtualmemory.cpp
  "${PROJECT_SOURCE_DIR}/src/libc/stdlib.cpp"
)

add_library(kernel_core_objects OBJECT ${KERNEL_CXX_SOURCES})
target_include_directories(kernel_core_objects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${KERNEL_ARCH_DIR}"
  "${PROJECT_SOURCE_DIR}/src/libc"
  "${PROJECT_SOURCE_DIR}/src/uapi"
  "${PROJECT_SOURCE_DIR}/third_party/font8x8"
)
set_target_properties(
  kernel_core_objects
  PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS ON
    CXX_STANDARD 20
    CXX_EXTENSIONS ON
)
target_compile_options(
  kernel_core_objects
  PRIVATE
    # Kernel code runs with asynchronous IRQs on the current stack, so the
    # SysV red zone is unsafe and must stay disabled across both boot paths.
    # The kernel does not enable or context-switch SIMD/FPU state yet. Force
    # integer-register-only codegen so newer cross-compilers cannot introduce
    # XMM/AVX instructions that trap as #UD on the CI runner's virtual CPU.
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding -mno-red-zone -march=x86-64 -mtune=generic -mgeneral-regs-only -O1 -Wall -Wextra -fno-asynchronous-unwind-tables>
    $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding -mno-red-zone -march=x86-64 -mtune=generic -mgeneral-regs-only -O0 -Wall -Wextra -fno-asynchronous-unwind-tables>
)

add_library(kernel_limine_frontend OBJECT "${PROJECT_SOURCE_DIR}/src/boot/limine/entry.cpp")
target_include_directories(kernel_limine_frontend PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${KERNEL_ARCH_DIR}"
  "${PROJECT_SOURCE_DIR}/src/libc"
  "${PROJECT_SOURCE_DIR}/src/uapi"
  "${PROJECT_SOURCE_DIR}/third_party/limine/v11.4.0"
)
set_target_properties(
  kernel_limine_frontend
  PROPERTIES
    CXX_STANDARD 20
    CXX_EXTENSIONS ON
)
target_compile_options(
  kernel_limine_frontend
  PRIVATE
    -ffreestanding
    -mno-red-zone
    -march=x86-64
    -mtune=generic
    -mgeneral-regs-only
    -mcmodel=large
    -O0
    -Wall
    -Wextra
    -fno-asynchronous-unwind-tables
    -fno-unwind-tables
    -fno-exceptions
    -fno-rtti
)

file(GLOB KERNEL_ASM_DEPENDENCIES CONFIGURE_DEPENDS
  "${KERNEL_ARCH_DIR}/*.asm"
  "${KERNEL_ARCH_DIR}/*.inc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.inc"
)
list(SORT KERNEL_ASM_DEPENDENCIES)

function(os1_add_nasm_object out_var source_name format)
  get_filename_component(source_stem "${source_name}" NAME_WE)
  set(output_path "${CMAKE_CURRENT_BINARY_DIR}/${source_stem}.o")
  add_custom_command(
    OUTPUT "${output_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND "${OS1_NASM_EXECUTABLE}" "-f${format}"
      -I "${KERNEL_ARCH_DIR}/"
      -I "${CMAKE_CURRENT_SOURCE_DIR}/"
      "${CMAKE_CURRENT_SOURCE_DIR}/${source_name}"
      -o "${output_path}"
    DEPENDS ${KERNEL_ASM_DEPENDENCIES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
  )
  set("${out_var}" "${output_path}" PARENT_SCOPE)
endfunction()

os1_add_nasm_object(KERNEL_MEMORY_OBJECT "arch/x86_64/memory.asm" "elf64")
os1_add_nasm_object(KERNEL_IRQHANDLER_OBJECT "arch/x86_64/irqhandler.asm" "elf64")
os1_add_nasm_object(KERNEL_INTHANDLER_OBJECT "arch/x86_64/inthandler.asm" "elf64")
os1_add_nasm_object(KERNEL_MULTITASK_OBJECT "arch/x86_64/multitask.asm" "elf64")
os1_add_nasm_object(KERNEL_CPUSTART_OBJECT "arch/x86_64/cpustart.asm" "elf64")

set(KERNEL_ASM_OBJECTS
  "${KERNEL_MEMORY_OBJECT}"
  "${KERNEL_IRQHANDLER_OBJECT}"
  "${KERNEL_INTHANDLER_OBJECT}"
  "${KERNEL_MULTITASK_OBJECT}"
  "${KERNEL_CPUSTART_OBJECT}"
)

add_custom_command(
  OUTPUT "${OS1_KERNEL_BIOS_ELF}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${CMAKE_LINKER}" -T "${KERNEL_BIOS_LINKER_SCRIPT}" $<TARGET_OBJECTS:kernel_core_objects> ${KERNEL_ASM_OBJECTS} -o "${OS1_KERNEL_BIOS_ELF}" -n
  DEPENDS
    kernel_core_objects
    $<TARGET_OBJECTS:kernel_core_objects>
    "${KERNEL_BIOS_LINKER_SCRIPT}"
    ${KERNEL_ASM_OBJECTS}
  COMMAND_EXPAND_LISTS
  VERBATIM
)

add_custom_command(
  OUTPUT "${OS1_KERNEL_LIMINE_ELF}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${CMAKE_LINKER}" -T "${KERNEL_LIMINE_LINKER_SCRIPT}" $<TARGET_OBJECTS:kernel_limine_frontend> -o "${OS1_KERNEL_LIMINE_ELF}" -n
  DEPENDS
    kernel_limine_frontend
    $<TARGET_OBJECTS:kernel_limine_frontend>
    "${KERNEL_LIMINE_LINKER_SCRIPT}"
  COMMAND_EXPAND_LISTS
  VERBATIM
)

add_custom_command(
  OUTPUT "${OS1_CPUSTART_BIN}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${OS1_NASM_EXECUTABLE}" -fbin
    -I "${KERNEL_ARCH_DIR}/"
    -I "${CMAKE_CURRENT_SOURCE_DIR}/"
    "${KERNEL_ARCH_DIR}/cpustart.asm"
    -o "${OS1_CPUSTART_BIN}"
  DEPENDS ${KERNEL_ASM_DEPENDENCIES}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM
)

add_custom_target(kernel_bios_elf DEPENDS "${OS1_KERNEL_BIOS_ELF}")
add_custom_target(kernel_limine_elf DEPENDS "${OS1_KERNEL_LIMINE_ELF}")
add_custom_target(cpustart_bin DEPENDS "${OS1_CPUSTART_BIN}")
