cmake_minimum_required(VERSION 3.20)

set(OS1_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchains/x86_64-elf.cmake")

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(OS1_CROSS_COMPILER_REQUESTED FALSE)

  foreach(OS1_COMPILER_VAR CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
    if(DEFINED ${OS1_COMPILER_VAR} AND "${${OS1_COMPILER_VAR}}" MATCHES "(^|/)x86_64-elf-g(cc|\\+\\+)$")
      set(OS1_CROSS_COMPILER_REQUESTED TRUE)
    endif()
  endforeach()

  if(OS1_CROSS_COMPILER_REQUESTED)
    include("${OS1_TOOLCHAIN_FILE}")
  endif()
endif()

project(os1 LANGUAGES C CXX)

if(NOT CMAKE_C_COMPILER MATCHES "(^|/)x86_64-elf-gcc$")
  message(FATAL_ERROR "os1 requires the x86_64-elf cross toolchain. Configure with -DCMAKE_TOOLCHAIN_FILE=${OS1_TOOLCHAIN_FILE} or use the provided CMake preset.")
endif()

if(NOT CMAKE_CXX_COMPILER MATCHES "(^|/)x86_64-elf-g\\+\\+$")
  message(FATAL_ERROR "os1 requires x86_64-elf-g++. Configure with -DCMAKE_TOOLCHAIN_FILE=${OS1_TOOLCHAIN_FILE} or use the provided CMake preset.")
endif()

foreach(OS1_LANG C CXX)
  foreach(OS1_CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_${OS1_LANG}_FLAGS_${OS1_CONFIG} "" CACHE STRING "Disabled for os1 cross-builds" FORCE)
  endforeach()
endforeach()

set(OS1_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts")
set(OS1_BOOT_BIN "${OS1_ARTIFACT_DIR}/boot.bin")
set(OS1_KERNEL16_BIN "${OS1_ARTIFACT_DIR}/kernel16.bin")
set(OS1_CPUSTART_BIN "${OS1_ARTIFACT_DIR}/cpustart.bin")
set(OS1_KERNEL64_ELF "${OS1_ARTIFACT_DIR}/kernel64.elf")
set(OS1_RAW_IMAGE "${OS1_ARTIFACT_DIR}/os1.raw")
set(OS1_SERIAL_LOG "${OS1_ARTIFACT_DIR}/os1.log")
set(OS1_SMOKE_LOG "${OS1_ARTIFACT_DIR}/smoke.log")
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
    "${OS1_BOOT_BIN}"
    "${OS1_KERNEL16_BIN}"
    "${OS1_KERNEL64_ELF}"
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
    DEPENDS "${OS1_KERNEL64_ELF}" "${OS1_CPUSTART_BIN}"
    VERBATIM
  )
else()
  os1_add_unavailable_target(disasm "disasm target unavailable: no compatible objdump was found")
endif()

include(CTest)

if(BUILD_TESTING AND OS1_QEMU_EXECUTABLE)
  set(OS1_SMOKE_MARKERS
    "[kernel64] hello!"
    "initializing page frame allocator"
    "Interrupts initialization successful"
    "start multitasking"
    "task table allocated at 0x"
    "new task 0x"
  )

  add_test(
    NAME os1_smoke
    COMMAND
      "${CMAKE_COMMAND}"
      "-DQEMU_EXECUTABLE=${OS1_QEMU_EXECUTABLE}"
      "-DRAW_IMAGE=${OS1_RAW_IMAGE}"
      "-DLOG_FILE=${OS1_SMOKE_LOG}"
      "-DSMOKE_TIMEOUT_SECONDS=8"
      "-DEXPECTED_MARKERS=${OS1_SMOKE_MARKERS}"
      -P
      "${PROJECT_SOURCE_DIR}/cmake/scripts/run_smoke_test.cmake"
  )

  add_custom_target(
    smoke
    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -R "^os1_smoke$"
    DEPENDS os1_image
    USES_TERMINAL
    VERBATIM
  )
else()
  os1_add_unavailable_target(smoke "smoke target unavailable: qemu-system-x86_64 was not found")
endif()

message(STATUS "Artifacts will be written to ${OS1_ARTIFACT_DIR}")
