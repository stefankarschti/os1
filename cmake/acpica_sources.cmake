if(NOT DEFINED OS1_REPO_ROOT)
  set(OS1_REPO_ROOT "${PROJECT_SOURCE_DIR}")
endif()

set(OS1_ACPICA_ROOT "${OS1_REPO_ROOT}/third_party/acpica")
set(OS1_ACPICA_CUSTOM_INCLUDE_DIR "${OS1_REPO_ROOT}/src/kernel/platform/acpica/include")
set(OS1_ACPICA_INCLUDE_DIR "${OS1_ACPICA_ROOT}/source/include")

if(NOT EXISTS "${OS1_ACPICA_ROOT}/source/include/acpi.h")
  message(FATAL_ERROR "Missing ACPICA sources under ${OS1_ACPICA_ROOT}; run git submodule update --init --recursive")
endif()

file(GLOB OS1_ACPICA_TABLE_SOURCES CONFIGURE_DEPENDS
  "${OS1_ACPICA_ROOT}/source/components/tables/*.c"
)
file(GLOB OS1_ACPICA_UTILITY_SOURCES CONFIGURE_DEPENDS
  "${OS1_ACPICA_ROOT}/source/components/utilities/*.c"
)

set(OS1_ACPICA_SOURCES
  ${OS1_ACPICA_TABLE_SOURCES}
  ${OS1_ACPICA_UTILITY_SOURCES}
)
list(SORT OS1_ACPICA_SOURCES)