if(NOT DEFINED INPUT_FILE OR NOT DEFINED MAX_BYTES OR NOT DEFINED SECTOR_COUNT OR NOT DEFINED LABEL OR NOT DEFINED VARIABLE_NAME OR NOT DEFINED CONFIG_FILE)
  message(FATAL_ERROR
    "assert_max_file_size.cmake requires INPUT_FILE, MAX_BYTES, SECTOR_COUNT, LABEL, VARIABLE_NAME, and CONFIG_FILE"
  )
endif()

if(NOT EXISTS "${INPUT_FILE}")
  message(FATAL_ERROR "Missing file for size assertion: ${INPUT_FILE}")
endif()

file(SIZE "${INPUT_FILE}" INPUT_BYTES)

if(INPUT_BYTES GREATER MAX_BYTES)
  math(EXPR REQUIRED_SECTORS "(${INPUT_BYTES} + 511) / 512")
  message(FATAL_ERROR
    "${LABEL} is ${INPUT_BYTES} bytes (${REQUIRED_SECTORS} sectors) but only ${MAX_BYTES} bytes (${SECTOR_COUNT} sectors) are reserved. Increase ${VARIABLE_NAME} in ${CONFIG_FILE} to expand the BIOS image slot and rebuild."
  )
endif()