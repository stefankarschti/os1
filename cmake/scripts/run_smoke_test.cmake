if(NOT DEFINED QEMU_EXECUTABLE)
  message(FATAL_ERROR "QEMU_EXECUTABLE must be set")
endif()

if(NOT DEFINED LOG_FILE)
  message(FATAL_ERROR "LOG_FILE must be set")
endif()

if(NOT DEFINED SMOKE_TIMEOUT_SECONDS)
  set(SMOKE_TIMEOUT_SECONDS 8)
endif()

if(NOT DEFINED EXPECTED_MARKERS)
  set(EXPECTED_MARKERS "")
endif()

set(qemu_command
  "${QEMU_EXECUTABLE}"
  -smp
  4
  -serial
  stdio
  -display
  none
  -no-reboot
  -no-shutdown
)

if(DEFINED ISO_IMAGE)
  if(NOT DEFINED OVMF_CODE)
    message(FATAL_ERROR "OVMF_CODE must be set for ISO-based smoke tests")
  endif()
  if(NOT DEFINED OVMF_VARS_TEMPLATE)
    message(FATAL_ERROR "OVMF_VARS_TEMPLATE must be set for ISO-based smoke tests")
  endif()

  get_filename_component(log_dir "${LOG_FILE}" DIRECTORY)
  file(MAKE_DIRECTORY "${log_dir}")
  set(ovmf_vars_copy "${log_dir}/smoke-ovmf-vars.fd")
  file(COPY_FILE "${OVMF_VARS_TEMPLATE}" "${ovmf_vars_copy}" ONLY_IF_DIFFERENT)

  list(APPEND qemu_command
    -machine
    q35
    -drive
    "if=pflash,format=raw,readonly=on,file=${OVMF_CODE}"
    -drive
    "if=pflash,format=raw,file=${ovmf_vars_copy}"
    -cdrom
    "${ISO_IMAGE}"
  )
elseif(DEFINED RAW_IMAGE)
  list(APPEND qemu_command
    -drive
    "format=raw,file=${RAW_IMAGE}"
  )
else()
  message(FATAL_ERROR "Either ISO_IMAGE or RAW_IMAGE must be set")
endif()

execute_process(
  COMMAND ${qemu_command}
  RESULT_VARIABLE qemu_result
  OUTPUT_VARIABLE qemu_stdout
  ERROR_VARIABLE qemu_stderr
  TIMEOUT "${SMOKE_TIMEOUT_SECONDS}"
)

set(qemu_output "${qemu_stdout}${qemu_stderr}")
file(WRITE "${LOG_FILE}" "${qemu_output}")

set(missing_markers "")
foreach(marker IN LISTS EXPECTED_MARKERS)
  string(FIND "${qemu_output}" "${marker}" marker_index)
  if(marker_index EQUAL -1)
    string(APPEND missing_markers "\n  ${marker}")
  endif()
endforeach()

if(missing_markers)
  message(FATAL_ERROR
    "Smoke test did not reach the expected boot markers:${missing_markers}\n"
    "QEMU result: ${qemu_result}\n"
    "See ${LOG_FILE} for the captured serial log."
  )
endif()
