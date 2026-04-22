set(KERNEL_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker2.ld")

set(KERNEL_CXX_SOURCES
  bootinfo.cpp
  cpu.cpp
  debug.cpp
  interrupt.cpp
  ioapic.cpp
  kernel.cpp
  keyboard.cpp
  lapic.cpp
  mp.cpp
  pageframe.cpp
  pic.cpp
  string.cpp
  task.cpp
  terminal.cpp
  virtualmemory.cpp
  "${PROJECT_SOURCE_DIR}/src/libc/stdlib.cpp"
)

add_library(kernel_objects OBJECT ${KERNEL_CXX_SOURCES})

target_include_directories(kernel_objects PRIVATE "${PROJECT_SOURCE_DIR}/src/libc")

set_target_properties(
  kernel_objects
  PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS ON
    CXX_STANDARD 20
    CXX_EXTENSIONS ON
)

target_compile_options(
  kernel_objects
  PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding -O1 -Wall -Wextra -fno-asynchronous-unwind-tables>
    $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding -O0 -Wall -Wextra -fno-asynchronous-unwind-tables>
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
  OUTPUT "${OS1_KERNEL64_ELF}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${CMAKE_LINKER}" -T "${KERNEL_LINKER_SCRIPT}" $<TARGET_OBJECTS:kernel_objects> ${KERNEL_ASM_OBJECTS} -o "${OS1_KERNEL64_ELF}" -n
  DEPENDS
    kernel_objects
    $<TARGET_OBJECTS:kernel_objects>
    "${KERNEL_LINKER_SCRIPT}"
    ${KERNEL_ASM_OBJECTS}
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

add_custom_target(kernel64_elf DEPENDS "${OS1_KERNEL64_ELF}")
add_custom_target(cpustart_bin DEPENDS "${OS1_CPUSTART_BIN}")
