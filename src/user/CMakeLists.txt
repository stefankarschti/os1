set(USER_STAGE_DIR "${CMAKE_BINARY_DIR}/user-root")
set(USER_BIN_DIR "${OS1_ARTIFACT_DIR}/user")
set(USER_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

set(USER_COMMON_CXX_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/crt0.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/syscall.cpp"
)

function(os1_add_user_cxx_program program_name source_file output_var)
  set(output_path "${USER_BIN_DIR}/${program_name}.elf")
  add_custom_command(
    OUTPUT "${output_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${USER_BIN_DIR}"
    COMMAND
      "${CMAKE_CXX_COMPILER}"
      -std=gnu++20
      -ffreestanding
      -fno-stack-protector
      -fno-asynchronous-unwind-tables
      -fno-unwind-tables
      -fno-exceptions
      -fno-rtti
      -fno-threadsafe-statics
      -fno-use-cxa-atexit
      -fno-pic
      -nostdinc++
      -march=x86-64
      -mtune=generic
      -mgeneral-regs-only
      -mcmodel=large
      -O1
      -Wall
      -Wextra
      "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
      "-I${PROJECT_SOURCE_DIR}/src/uapi"
      -nostdlib
      -nostartfiles
      -nodefaultlibs
      "-Wl,-T,${USER_LINKER_SCRIPT}"
      -Wl,-static
      -Wl,-n
      -Wl,--build-id=none
      -o "${output_path}"
      ${USER_COMMON_CXX_SOURCES}
      "${source_file}"
    DEPENDS
      ${USER_COMMON_CXX_SOURCES}
      "${source_file}"
      "${USER_LINKER_SCRIPT}"
    VERBATIM
  )
  set("${output_var}" "${output_path}" PARENT_SCOPE)
endfunction()

os1_add_user_cxx_program(sh "${CMAKE_CURRENT_SOURCE_DIR}/programs/sh.cpp" OS1_USER_SH_ELF)
os1_add_user_cxx_program(init "${CMAKE_CURRENT_SOURCE_DIR}/programs/init.cpp" OS1_USER_INIT_ELF)
os1_add_user_cxx_program(yield "${CMAKE_CURRENT_SOURCE_DIR}/programs/yield.cpp" OS1_USER_YIELD_ELF)
os1_add_user_cxx_program(fault "${CMAKE_CURRENT_SOURCE_DIR}/programs/fault.cpp" OS1_USER_FAULT_ELF)
os1_add_user_cxx_program(copycheck "${CMAKE_CURRENT_SOURCE_DIR}/programs/copycheck.cpp" OS1_USER_COPYCHECK_ELF)

add_custom_command(
  OUTPUT "${OS1_INITRD_CPIO}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${USER_STAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${USER_STAGE_DIR}/bin"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_USER_INIT_ELF}" "${USER_STAGE_DIR}/bin/init"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_USER_SH_ELF}" "${USER_STAGE_DIR}/bin/sh"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_USER_YIELD_ELF}" "${USER_STAGE_DIR}/bin/yield"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_USER_FAULT_ELF}" "${USER_STAGE_DIR}/bin/fault"
  COMMAND "${CMAKE_COMMAND}" -E copy "${OS1_USER_COPYCHECK_ELF}" "${USER_STAGE_DIR}/bin/copycheck"
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSTAGE_DIR=${USER_STAGE_DIR}"
    "-DOUTPUT_FILE=${OS1_INITRD_CPIO}"
    "-DCPIO_EXECUTABLE=${OS1_CPIO_EXECUTABLE}"
    -P
    "${PROJECT_SOURCE_DIR}/cmake/scripts/create_initrd.cmake"
  DEPENDS
    "${OS1_USER_INIT_ELF}"
    "${OS1_USER_SH_ELF}"
    "${OS1_USER_YIELD_ELF}"
    "${OS1_USER_FAULT_ELF}"
    "${OS1_USER_COPYCHECK_ELF}"
  VERBATIM
)

add_custom_target(user_programs DEPENDS
  "${OS1_USER_INIT_ELF}"
  "${OS1_USER_SH_ELF}"
  "${OS1_USER_YIELD_ELF}"
  "${OS1_USER_FAULT_ELF}"
  "${OS1_USER_COPYCHECK_ELF}"
)

add_custom_target(initrd_cpio DEPENDS "${OS1_INITRD_CPIO}")
