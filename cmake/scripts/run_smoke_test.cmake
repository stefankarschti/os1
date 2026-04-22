if(NOT DEFINED QEMU_EXECUTABLE)
  message(FATAL_ERROR "QEMU_EXECUTABLE must be set")
endif()

if(NOT DEFINED RAW_IMAGE)
  message(FATAL_ERROR "RAW_IMAGE must be set")
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

execute_process(
  COMMAND
    "${QEMU_EXECUTABLE}"
    -smp 4
    -drive "format=raw,file=${RAW_IMAGE}"
    -serial stdio
    -display none
    -no-reboot
    -no-shutdown
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
