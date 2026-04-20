if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE must be set")
endif()

if(NOT DEFINED COMMAND_ARGS)
  message(FATAL_ERROR "COMMAND_ARGS must be set")
endif()

execute_process(
  COMMAND ${COMMAND_ARGS}
  RESULT_VARIABLE command_result
  OUTPUT_FILE "${OUTPUT_FILE}"
)

if(NOT command_result EQUAL 0)
  message(FATAL_ERROR "Command failed while generating ${OUTPUT_FILE}")
endif()
