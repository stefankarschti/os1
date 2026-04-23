if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE must be set")
endif()

set(sector_size 512)

function(make_sector out_var prefix)
  string(LENGTH "${prefix}" prefix_length)
  if(prefix_length GREATER sector_size)
    message(FATAL_ERROR "Sector prefix is too long: ${prefix}")
  endif()

  math(EXPR padding_length "${sector_size} - ${prefix_length}")
  string(REPEAT "." "${padding_length}" padding)
  set(${out_var} "${prefix}${padding}" PARENT_SCOPE)
endfunction()

make_sector(sector0 "OS1 VIRTIO TEST DISK SECTOR 0 SIGNATURE")
make_sector(sector1 "OS1 VIRTIO TEST DISK SECTOR 1 PAYLOAD")
make_sector(sector2 "OS1 VIRTIO TEST DISK SECTOR 2 RESERVED")
make_sector(sector3 "OS1 VIRTIO TEST DISK SECTOR 3 RESERVED")

file(WRITE "${OUTPUT_FILE}" "${sector0}${sector1}${sector2}${sector3}")
