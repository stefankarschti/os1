set(KERNEL_BIOS_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker2.ld")
set(KERNEL_LIMINE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker_limine.ld")

set(KERNEL_CXX_SOURCES
  bootinfo.cpp
  console_input.cpp
  cpu.cpp
  debug.cpp
  display.cpp
  interrupt.cpp
  ioapic.cpp
  kernel.cpp
  keyboard.cpp
  lapic.cpp
  mp.cpp
  pageframe.cpp
  pic.cpp
  platform.cpp
  string.cpp
  task.cpp
  terminal.cpp
  virtualmemory.cpp
  "${PROJECT_SOURCE_DIR}/src/libc/stdlib.cpp"
)

add_library(kernel_core_objects OBJECT ${KERNEL_CXX_SOURCES})
target_include_directories(kernel_core_objects PRIVATE
  "${PROJECT_SOURCE_DIR}/src/libc"
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
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding -mno-red-zone -O1 -Wall -Wextra -fno-asynchronous-unwind-tables>
    $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding -mno-red-zone -O0 -Wall -Wextra -fno-asynchronous-unwind-tables>
)

add_library(kernel_limine_frontend OBJECT "${PROJECT_SOURCE_DIR}/src/boot/limine/entry.cpp")
target_include_directories(kernel_limine_frontend PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/src/libc"
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
    -mcmodel=large
    -O0
    -Wall
    -Wextra
    -fno-asynchronous-unwind-tables
    -fno-unwind-tables
    -fno-exceptions
    -fno-rtti
)

file(GLOB KERNEL_ASM_DEPENDENCIES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.asm" "${CMAKE_CURRENT_SOURCE_DIR}/*.inc")
list(SORT KERNEL_ASM_DEPENDENCIES)

function(os1_add_nasm_object out_var source_name format)
  set(output_path "${CMAKE_CURRENT_BINARY_DIR}/${source_name}.o")
  add_custom_command(
    OUTPUT "${output_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND "${OS1_NASM_EXECUTABLE}" "-f${format}" "${CMAKE_CURRENT_SOURCE_DIR}/${source_name}" -o "${output_path}"
    DEPENDS ${KERNEL_ASM_DEPENDENCIES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
  )
  set("${out_var}" "${output_path}" PARENT_SCOPE)
endfunction()

os1_add_nasm_object(KERNEL_MEMORY_OBJECT "memory.asm" "elf64")
os1_add_nasm_object(KERNEL_IRQHANDLER_OBJECT "irqhandler.asm" "elf64")
os1_add_nasm_object(KERNEL_INTHANDLER_OBJECT "inthandler.asm" "elf64")
os1_add_nasm_object(KERNEL_MULTITASK_OBJECT "multitask.asm" "elf64")
os1_add_nasm_object(KERNEL_CPUSTART_OBJECT "cpustart.asm" "elf64")

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
  COMMAND "${OS1_NASM_EXECUTABLE}" -fbin "${CMAKE_CURRENT_SOURCE_DIR}/cpustart.asm" -o "${OS1_CPUSTART_BIN}"
  DEPENDS ${KERNEL_ASM_DEPENDENCIES}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM
)

add_custom_target(kernel_bios_elf DEPENDS "${OS1_KERNEL_BIOS_ELF}")
add_custom_target(kernel_limine_elf DEPENDS "${OS1_KERNEL_LIMINE_ELF}")
add_custom_target(cpustart_bin DEPENDS "${OS1_CPUSTART_BIN}")
