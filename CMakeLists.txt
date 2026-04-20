cmake_minimum_required(VERSION 3.20)

project(os1 LANGUAGES C CXX)

set(OS1_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts")
set(OS1_BOOT_BIN "${OS1_ARTIFACT_DIR}/boot.bin")
set(OS1_KERNEL16_BIN "${OS1_ARTIFACT_DIR}/kernel16.bin")
set(OS1_CPUSTART_BIN "${OS1_ARTIFACT_DIR}/cpustart.bin")
set(OS1_KERNEL64_ELF "${OS1_ARTIFACT_DIR}/kernel64.elf")
set(OS1_RAW_IMAGE "${OS1_ARTIFACT_DIR}/os1.raw")
set(OS1_SERIAL_LOG "${OS1_ARTIFACT_DIR}/os1.log")
set(OS1_DUMP_ASM "${OS1_ARTIFACT_DIR}/dump.asm")
set(OS1_CPUSTART_DUMP "${OS1_ARTIFACT_DIR}/cpustart.asm")

find_program(OS1_NASM_EXECUTABLE NAMES nasm REQUIRED)
find_program(OS1_DD_EXECUTABLE NAMES dd REQUIRED)
find_program(OS1_QEMU_EXECUTABLE NAMES qemu-system-x86_64)
find_program(OS1_OBJDUMP_EXECUTABLE NAMES x86_64-elf-objdump objdump)

add_subdirectory(src/boot)
add_subdirectory(src/kernel)

add_custom_command(
  OUTPUT "${OS1_RAW_IMAGE}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${OS1_DD_EXECUTABLE}" if=/dev/zero "of=${OS1_RAW_IMAGE}" bs=1048576 count=1
  COMMAND "${OS1_DD_EXECUTABLE}" "if=${OS1_BOOT_BIN}" "of=${OS1_RAW_IMAGE}" bs=512 count=1 conv=notrunc,sync
  COMMAND "${OS1_DD_EXECUTABLE}" "if=${OS1_KERNEL16_BIN}" "of=${OS1_RAW_IMAGE}" seek=1 count=8 conv=notrunc,sync
  COMMAND "${OS1_DD_EXECUTABLE}" "if=${OS1_KERNEL64_ELF}" "of=${OS1_RAW_IMAGE}" bs=512 seek=9 count=256 conv=notrunc,sync
  DEPENDS
    boot_bin
    kernel16_bin
    kernel64_elf
  VERBATIM
)

add_custom_target(os1_image ALL DEPENDS "${OS1_RAW_IMAGE}")

function(os1_add_unavailable_target target_name reason)
  add_custom_target(
    "${target_name}"
    COMMAND "${CMAKE_COMMAND}" -E echo "${reason}"
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM
  )
endfunction()

if(OS1_QEMU_EXECUTABLE)
  add_custom_target(
    run
    BYPRODUCTS "${OS1_SERIAL_LOG}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
    COMMAND "${OS1_QEMU_EXECUTABLE}"
            -smp 4
            "-drive"
            "format=raw,file=${OS1_RAW_IMAGE}"
            "-serial"
            "file:${OS1_SERIAL_LOG}"
            -d cpu_reset
            -no-reboot
            -d int
    DEPENDS os1_image
    USES_TERMINAL
    VERBATIM
  )
else()
  os1_add_unavailable_target(run "run target unavailable: qemu-system-x86_64 was not found")
endif()

if(OS1_OBJDUMP_EXECUTABLE)
  add_custom_target(
    disasm
    BYPRODUCTS "${OS1_DUMP_ASM}" "${OS1_CPUSTART_DUMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DOUTPUT_FILE=${OS1_DUMP_ASM}"
      "-DCOMMAND_ARGS=${OS1_OBJDUMP_EXECUTABLE};-d;-M;intel;${OS1_KERNEL64_ELF}"
      -P
      "${PROJECT_SOURCE_DIR}/cmake/scripts/write_stdout_to_file.cmake"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DOUTPUT_FILE=${OS1_CPUSTART_DUMP}"
      "-DCOMMAND_ARGS=${OS1_OBJDUMP_EXECUTABLE};-D;-b;binary;-m;i8086;-M;intel;${OS1_CPUSTART_BIN}"
      -P
      "${PROJECT_SOURCE_DIR}/cmake/scripts/write_stdout_to_file.cmake"
    DEPENDS kernel64_elf cpustart_bin
    VERBATIM
  )
else()
  os1_add_unavailable_target(disasm "disasm target unavailable: no compatible objdump was found")
endif()

message(STATUS "Artifacts will be written to ${OS1_ARTIFACT_DIR}")
