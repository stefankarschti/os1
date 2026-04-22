if(NOT DEFINED STAGE_DIR)
  message(FATAL_ERROR "STAGE_DIR must be set")
endif()

if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE must be set")
endif()

if(NOT DEFINED CPIO_EXECUTABLE)
  message(FATAL_ERROR "CPIO_EXECUTABLE must be set")
endif()

execute_process(
  COMMAND /bin/sh -c "find . -print | \"${CPIO_EXECUTABLE}\" -o -H newc > \"${OUTPUT_FILE}\""
  WORKING_DIRECTORY "${STAGE_DIR}"
  RESULT_VARIABLE initrd_result
)

if(NOT initrd_result EQUAL 0)
  message(FATAL_ERROR "Failed to create initrd archive at ${OUTPUT_FILE}")
endif()
