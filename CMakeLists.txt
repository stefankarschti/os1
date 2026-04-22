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

function(os1_detect_existing_file out_var)
  foreach(candidate ${ARGN})
    if(candidate AND EXISTS "${candidate}")
      set(${out_var} "${candidate}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${out_var} "" PARENT_SCOPE)
endfunction()

set(OS1_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts")
set(OS1_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(OS1_ISO_STAGE_DIR "${CMAKE_BINARY_DIR}/iso-root")

set(OS1_BOOT_BIN "${OS1_ARTIFACT_DIR}/boot.bin")
set(OS1_KERNEL16_BIN "${OS1_ARTIFACT_DIR}/kernel16.bin")
set(OS1_CPUSTART_BIN "${OS1_ARTIFACT_DIR}/cpustart.bin")
set(OS1_KERNEL_BIOS_ELF "${OS1_ARTIFACT_DIR}/kernel_bios.elf")
set(OS1_KERNEL_LIMINE_ELF "${OS1_ARTIFACT_DIR}/kernel_limine.elf")
set(OS1_INITRD_CPIO "${OS1_ARTIFACT_DIR}/initrd.cpio")
set(OS1_RAW_IMAGE "${OS1_ARTIFACT_DIR}/os1.raw")
set(OS1_ISO_IMAGE "${OS1_ARTIFACT_DIR}/os1.iso")
set(OS1_SERIAL_LOG "${OS1_ARTIFACT_DIR}/os1.log")
set(OS1_BIOS_SERIAL_LOG "${OS1_ARTIFACT_DIR}/os1-bios.log")
set(OS1_OVMF_VARS_FILE "${OS1_ARTIFACT_DIR}/ovmf-vars.fd")
set(OS1_SMOKE_LOG "${OS1_ARTIFACT_DIR}/smoke.log")
set(OS1_SMOKE_BIOS_LOG "${OS1_ARTIFACT_DIR}/smoke-bios.log")
set(OS1_DUMP_ASM "${OS1_ARTIFACT_DIR}/dump.asm")
set(OS1_CPUSTART_DUMP "${OS1_ARTIFACT_DIR}/cpustart.asm")

set(OS1_LIMINE_DIR "${PROJECT_SOURCE_DIR}/third_party/limine/v11.4.0")
set(OS1_LIMINE_BOOTX64 "${OS1_LIMINE_DIR}/BOOTX64.EFI")
set(OS1_LIMINE_UEFI_CD "${OS1_LIMINE_DIR}/limine-uefi-cd.bin")
set(OS1_LIMINE_PROTOCOL_HEADER "${OS1_LIMINE_DIR}/limine.h")
set(OS1_LIMINE_CONFIG_SOURCE "${PROJECT_SOURCE_DIR}/src/boot/limine/limine.conf")

foreach(OS1_VENDOR_FILE
    "${OS1_LIMINE_BOOTX64}"
    "${OS1_LIMINE_UEFI_CD}"
    "${OS1_LIMINE_PROTOCOL_HEADER}"
    "${OS1_LIMINE_CONFIG_SOURCE}")
  if(NOT EXISTS "${OS1_VENDOR_FILE}")
    message(FATAL_ERROR "Missing required vendored Limine input: ${OS1_VENDOR_FILE}")
  endif()
endforeach()

set(OS1_LOADER16_IMAGE_START_LBA 1)
set(OS1_LOADER16_IMAGE_SECTOR_COUNT 64)
math(EXPR OS1_KERNEL_IMAGE_START_LBA "${OS1_LOADER16_IMAGE_START_LBA} + ${OS1_LOADER16_IMAGE_SECTOR_COUNT}")
set(OS1_KERNEL_IMAGE_SECTOR_COUNT 256)
math(EXPR OS1_INITRD_IMAGE_START_LBA "${OS1_KERNEL_IMAGE_START_LBA} + ${OS1_KERNEL_IMAGE_SECTOR_COUNT}")
set(OS1_INITRD_IMAGE_SECTOR_COUNT 128)

set(OS1_IMAGE_LAYOUT_INC "${OS1_GENERATED_DIR}/image_layout.inc")
configure_file(
  "${PROJECT_SOURCE_DIR}/cmake/templates/image_layout.inc.in"
  "${OS1_IMAGE_LAYOUT_INC}"
  @ONLY
)

find_program(OS1_NASM_EXECUTABLE NAMES nasm REQUIRED)
find_program(OS1_DD_EXECUTABLE NAMES dd REQUIRED)
find_program(OS1_CPIO_EXECUTABLE NAMES cpio REQUIRED)
find_program(OS1_XORRISO_EXECUTABLE NAMES xorriso REQUIRED)
find_program(OS1_QEMU_EXECUTABLE NAMES qemu-system-x86_64)
find_program(OS1_OBJDUMP_EXECUTABLE NAMES x86_64-elf-objdump objdump)

set(OS1_OVMF_CODE "${OS1_OVMF_CODE}" CACHE FILEPATH "Path to the OVMF/edk2 code firmware image used for UEFI run and smoke targets")
set(OS1_OVMF_VARS_TEMPLATE "${OS1_OVMF_VARS_TEMPLATE}" CACHE FILEPATH "Path to the OVMF/edk2 vars template used for UEFI run and smoke targets")

if(OS1_QEMU_EXECUTABLE)
  get_filename_component(OS1_QEMU_BIN_DIR "${OS1_QEMU_EXECUTABLE}" DIRECTORY)
  get_filename_component(OS1_QEMU_PREFIX "${OS1_QEMU_BIN_DIR}/.." ABSOLUTE)

  if(NOT OS1_OVMF_CODE)
    os1_detect_existing_file(OS1_OVMF_CODE_DETECTED
      "${OS1_QEMU_PREFIX}/share/qemu/edk2-x86_64-code.fd"
      "${OS1_QEMU_PREFIX}/share/qemu/OVMF_CODE.fd"
      "/usr/share/OVMF/OVMF_CODE.fd"
      "/usr/share/OVMF/OVMF_CODE_4M.fd"
      "/usr/share/edk2/ovmf/OVMF_CODE.fd"
      "/usr/share/edk2/ovmf/OVMF_CODE_4M.fd"
      "/usr/share/qemu/OVMF_CODE.fd"
      "/usr/share/qemu/edk2-x86_64-code.fd"
    )
    if(OS1_OVMF_CODE_DETECTED)
      set(OS1_OVMF_CODE "${OS1_OVMF_CODE_DETECTED}" CACHE FILEPATH "Path to the OVMF/edk2 code firmware image used for UEFI run and smoke targets" FORCE)
    endif()
  endif()

  if(NOT OS1_OVMF_VARS_TEMPLATE)
    os1_detect_existing_file(OS1_OVMF_VARS_TEMPLATE_DETECTED
      "${OS1_QEMU_PREFIX}/share/qemu/edk2-x86_64-vars.fd"
      "${OS1_QEMU_PREFIX}/share/qemu/edk2-i386-vars.fd"
      "${OS1_QEMU_PREFIX}/share/qemu/OVMF_VARS.fd"
      "/usr/share/OVMF/OVMF_VARS.fd"
      "/usr/share/OVMF/OVMF_VARS_4M.fd"
      "/usr/share/edk2/ovmf/OVMF_VARS.fd"
      "/usr/share/edk2/ovmf/OVMF_VARS_4M.fd"
      "/usr/share/qemu/OVMF_VARS.fd"
      "/usr/share/qemu/edk2-x86_64-vars.fd"
      "/usr/share/qemu/edk2-i386-vars.fd"
    )
    if(OS1_OVMF_VARS_TEMPLATE_DETECTED)
      set(OS1_OVMF_VARS_TEMPLATE "${OS1_OVMF_VARS_TEMPLATE_DETECTED}" CACHE FILEPATH "Path to the OVMF/edk2 vars template used for UEFI run and smoke targets" FORCE)
    endif()
  endif()
endif()

add_subdirectory(src/boot)
add_subdirectory(src/kernel)
add_subdirectory(src/user)

add_custom_command(
  OUTPUT "${OS1_RAW_IMAGE}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${OS1_DD_EXECUTABLE}" if=/dev/zero "of=${OS1_RAW_IMAGE}" bs=1048576 count=1
  COMMAND "${OS1_DD_EXECUTABLE}" "if=${OS1_BOOT_BIN}" "of=${OS1_RAW_IMAGE}" bs=512 count=1 conv=notrunc,sync
  COMMAND
    "${OS1_DD_EXECUTABLE}"
    "if=${OS1_KERNEL16_BIN}"
    "of=${OS1_RAW_IMAGE}"
    "seek=${OS1_LOADER16_IMAGE_START_LBA}"
    "count=${OS1_LOADER16_IMAGE_SECTOR_COUNT}"
    conv=notrunc,sync
  COMMAND
    "${OS1_DD_EXECUTABLE}"
    "if=${OS1_KERNEL_BIOS_ELF}"
    "of=${OS1_RAW_IMAGE}"
    bs=512
    "seek=${OS1_KERNEL_IMAGE_START_LBA}"
    "count=${OS1_KERNEL_IMAGE_SECTOR_COUNT}"
    conv=notrunc,sync
  COMMAND
    "${OS1_DD_EXECUTABLE}"
    "if=${OS1_INITRD_CPIO}"
    "of=${OS1_RAW_IMAGE}"
    bs=512
    "seek=${OS1_INITRD_IMAGE_START_LBA}"
    "count=${OS1_INITRD_IMAGE_SECTOR_COUNT}"
    conv=notrunc,sync
  DEPENDS
    "${OS1_BOOT_BIN}"
    "${OS1_KERNEL16_BIN}"
    "${OS1_KERNEL_BIOS_ELF}"
    "${OS1_INITRD_CPIO}"
  VERBATIM
)

add_custom_command(
  OUTPUT "${OS1_ISO_IMAGE}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${OS1_ISO_STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ISO_STAGE_DIR}/EFI/BOOT"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ISO_STAGE_DIR}/boot/limine"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_LIMINE_BOOTX64}" "${OS1_ISO_STAGE_DIR}/EFI/BOOT/BOOTX64.EFI"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_LIMINE_CONFIG_SOURCE}" "${OS1_ISO_STAGE_DIR}/limine.conf"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_LIMINE_CONFIG_SOURCE}" "${OS1_ISO_STAGE_DIR}/EFI/BOOT/limine.conf"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_LIMINE_UEFI_CD}" "${OS1_ISO_STAGE_DIR}/boot/limine/limine-uefi-cd.bin"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_KERNEL_BIOS_ELF}" "${OS1_ISO_STAGE_DIR}/kernel_bios.elf"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_KERNEL_LIMINE_ELF}" "${OS1_ISO_STAGE_DIR}/kernel_limine.elf"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_INITRD_CPIO}" "${OS1_ISO_STAGE_DIR}/initrd.cpio"
  COMMAND
    "${OS1_XORRISO_EXECUTABLE}"
    -as
    mkisofs
    -R
    -r
    --efi-boot
    boot/limine/limine-uefi-cd.bin
    -efi-boot-part
    --efi-boot-image
    --protective-msdos-label
    -no-emul-boot
    -o
    "${OS1_ISO_IMAGE}"
    "${OS1_ISO_STAGE_DIR}"
  DEPENDS
    "${OS1_LIMINE_BOOTX64}"
    "${OS1_LIMINE_UEFI_CD}"
    "${OS1_LIMINE_CONFIG_SOURCE}"
    "${OS1_KERNEL_BIOS_ELF}"
    "${OS1_KERNEL_LIMINE_ELF}"
    "${OS1_INITRD_CPIO}"
  VERBATIM
)

add_custom_target(os1_image ALL DEPENDS "${OS1_ISO_IMAGE}")
add_custom_target(os1_bios_image DEPENDS "${OS1_RAW_IMAGE}")

function(os1_add_unavailable_target target_name reason)
  add_custom_target(
    "${target_name}"
    COMMAND "${CMAKE_COMMAND}" -E echo "${reason}"
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM
  )
endfunction()

if(OS1_QEMU_EXECUTABLE AND OS1_OVMF_CODE AND OS1_OVMF_VARS_TEMPLATE)
  add_custom_target(
    run
    BYPRODUCTS "${OS1_SERIAL_LOG}" "${OS1_OVMF_VARS_FILE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_OVMF_VARS_TEMPLATE}" "${OS1_OVMF_VARS_FILE}"
    COMMAND
      "${OS1_QEMU_EXECUTABLE}"
      -machine
      q35
      -smp
      4
      -drive
      "if=pflash,format=raw,readonly=on,file=${OS1_OVMF_CODE}"
      -drive
      "if=pflash,format=raw,file=${OS1_OVMF_VARS_FILE}"
      -cdrom
      "${OS1_ISO_IMAGE}"
      -serial
      "file:${OS1_SERIAL_LOG}"
      -no-reboot
      -d
      cpu_reset
      -d
      int
    DEPENDS os1_image
    USES_TERMINAL
    VERBATIM
  )
else()
  os1_add_unavailable_target(run "run target unavailable: qemu-system-x86_64 or OVMF firmware was not found")
endif()

if(OS1_QEMU_EXECUTABLE)
  add_custom_target(
    run_bios
    BYPRODUCTS "${OS1_BIOS_SERIAL_LOG}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
    COMMAND
      "${OS1_QEMU_EXECUTABLE}"
      -smp
      4
      -drive
      "format=raw,file=${OS1_RAW_IMAGE}"
      -serial
      "file:${OS1_BIOS_SERIAL_LOG}"
      -no-reboot
      -d
      cpu_reset
      -d
      int
    DEPENDS os1_bios_image
    USES_TERMINAL
    VERBATIM
  )
else()
  os1_add_unavailable_target(run_bios "run_bios target unavailable: qemu-system-x86_64 was not found")
endif()

if(OS1_OBJDUMP_EXECUTABLE)
  add_custom_target(
    disasm
    BYPRODUCTS "${OS1_DUMP_ASM}" "${OS1_CPUSTART_DUMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OS1_ARTIFACT_DIR}"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DOUTPUT_FILE=${OS1_DUMP_ASM}"
      "-DCOMMAND_ARGS=${OS1_OBJDUMP_EXECUTABLE};-d;-M;intel;${OS1_KERNEL_LIMINE_ELF}"
      -P
      "${PROJECT_SOURCE_DIR}/cmake/scripts/write_stdout_to_file.cmake"
    COMMAND
      "${CMAKE_COMMAND}"
      "-DOUTPUT_FILE=${OS1_CPUSTART_DUMP}"
      "-DCOMMAND_ARGS=${OS1_OBJDUMP_EXECUTABLE};-D;-b;binary;-m;i8086;-M;intel;${OS1_CPUSTART_BIN}"
      -P
      "${PROJECT_SOURCE_DIR}/cmake/scripts/write_stdout_to_file.cmake"
    DEPENDS "${OS1_KERNEL_LIMINE_ELF}" "${OS1_CPUSTART_BIN}"
    VERBATIM
  )
else()
  os1_add_unavailable_target(disasm "disasm target unavailable: no compatible objdump was found")
endif()

include(CTest)

# Both smoke tests are intentionally "run until timeout, then inspect the log"
# checks. In practice they currently reach their success markers very close to
# the old 8s limit on some hosts, especially through GUI tooling. Give them a
# little headroom so BIOS compatibility failures reflect real boot breakage
# rather than timing jitter.
set(OS1_UEFI_SMOKE_TIMEOUT_SECONDS 12)
set(OS1_BIOS_SMOKE_TIMEOUT_SECONDS 12)

if(BUILD_TESTING AND OS1_QEMU_EXECUTABLE AND OS1_OVMF_CODE AND OS1_OVMF_VARS_TEMPLATE)
  set(OS1_UEFI_SMOKE_MARKERS
    "[kernel64] hello!"
    "boot source: limine"
    "framebuffer console active"
    "initrd module discovered"
    "starting first user process"
    "[user/init] hello"
    "[user/yield] tick 0"
    "user page fault killed pid"
    "idle thread online"
  )

  add_test(
    NAME os1_smoke
    COMMAND
      "${CMAKE_COMMAND}"
      "-DQEMU_EXECUTABLE=${OS1_QEMU_EXECUTABLE}"
      "-DISO_IMAGE=${OS1_ISO_IMAGE}"
      "-DOVMF_CODE=${OS1_OVMF_CODE}"
      "-DOVMF_VARS_TEMPLATE=${OS1_OVMF_VARS_TEMPLATE}"
      "-DLOG_FILE=${OS1_SMOKE_LOG}"
      "-DSMOKE_TIMEOUT_SECONDS=${OS1_UEFI_SMOKE_TIMEOUT_SECONDS}"
      "-DEXPECTED_MARKERS=${OS1_UEFI_SMOKE_MARKERS}"
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
  os1_add_unavailable_target(smoke "smoke target unavailable: qemu-system-x86_64 or OVMF firmware was not found")
endif()

if(BUILD_TESTING AND OS1_QEMU_EXECUTABLE)
  set(OS1_BIOS_SMOKE_MARKERS
    "[kernel64] hello!"
    "boot source: bios"
    "Interrupts initialization successful"
    "initrd module discovered"
    "starting first user process"
    "[user/init] hello"
    "[user/yield] tick 0"
    "user page fault killed pid"
    "idle thread online"
  )

  add_test(
    NAME os1_smoke_bios
    COMMAND
      "${CMAKE_COMMAND}"
      "-DQEMU_EXECUTABLE=${OS1_QEMU_EXECUTABLE}"
      "-DRAW_IMAGE=${OS1_RAW_IMAGE}"
      "-DLOG_FILE=${OS1_SMOKE_BIOS_LOG}"
      "-DSMOKE_TIMEOUT_SECONDS=${OS1_BIOS_SMOKE_TIMEOUT_SECONDS}"
      "-DEXPECTED_MARKERS=${OS1_BIOS_SMOKE_MARKERS}"
      -P
      "${PROJECT_SOURCE_DIR}/cmake/scripts/run_smoke_test.cmake"
  )

  add_custom_target(
    smoke_bios
    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -R "^os1_smoke_bios$"
    DEPENDS os1_bios_image
    USES_TERMINAL
    VERBATIM
  )
else()
  os1_add_unavailable_target(smoke_bios "smoke_bios target unavailable: qemu-system-x86_64 was not found")
endif()

if(BUILD_TESTING AND OS1_QEMU_EXECUTABLE AND OS1_OVMF_CODE AND OS1_OVMF_VARS_TEMPLATE)
  add_custom_target(
    smoke_all
    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -R "^os1_smoke(_bios)?$"
    DEPENDS os1_image os1_bios_image
    USES_TERMINAL
    VERBATIM
  )
else()
  os1_add_unavailable_target(smoke_all "smoke_all target unavailable: qemu-system-x86_64 or OVMF firmware was not found")
endif()

message(STATUS "Artifacts will be written to ${OS1_ARTIFACT_DIR}")
message(STATUS "Default image: ${OS1_ISO_IMAGE}")
message(STATUS "Legacy BIOS image: ${OS1_RAW_IMAGE}")
