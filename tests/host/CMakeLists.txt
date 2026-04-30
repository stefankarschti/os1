cmake_minimum_required(VERSION 3.20)

project(os1_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(OS1_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(OS1_HOST_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

set(OS1_KERNEL_IMAGE_LOAD_ADDRESS 0x10000)
set(OS1_INITRD_LOAD_ADDRESS 0x80000)
set(OS1_KERNEL_RESERVED_PHYSICAL_START 0x100000)
set(OS1_KERNEL_RESERVED_PHYSICAL_END 0x160000)
set(OS1_KERNEL_VIRTUAL_OFFSET 0xFFFFFFFF80000000)
set(OS1_KERNEL_SHIM_VIRTUAL_BASE 0xFFFFFFFF90000000)
set(OS1_DIRECT_MAP_BASE 0xFFFF800000000000)
math(EXPR OS1_KERNEL_RESERVED_PHYSICAL_BYTES
  "${OS1_KERNEL_RESERVED_PHYSICAL_END} - ${OS1_KERNEL_RESERVED_PHYSICAL_START}")
set(OS1_KERNEL_POST_IMAGE_RESERVE_BYTES 0x3000)

file(MAKE_DIRECTORY "${OS1_HOST_GENERATED_DIR}")
configure_file(
  "${OS1_REPO_ROOT}/cmake/templates/kernel_layout.hpp.in"
  "${OS1_HOST_GENERATED_DIR}/kernel_layout.hpp"
  @ONLY
)

enable_testing()

set(INSTALL_GTEST OFF CACHE BOOL "Do not install GoogleTest from os1 host tests" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory("${OS1_REPO_ROOT}/third_party/googletest" "${CMAKE_BINARY_DIR}/googletest")

add_library(os1_host_support
  support/debug_stub.cpp
  support/memory_stubs.cpp
  support/physical_memory.cpp
  "${OS1_REPO_ROOT}/src/kernel/arch/x86_64/interrupt/vector_allocator.cpp"
  "${OS1_REPO_ROOT}/src/kernel/core/timer_source.cpp"
  "${OS1_REPO_ROOT}/src/kernel/debug/event_ring.cpp"
  "${OS1_REPO_ROOT}/src/kernel/drivers/bus/device.cpp"
  "${OS1_REPO_ROOT}/src/kernel/drivers/bus/driver_registry.cpp"
  "${OS1_REPO_ROOT}/src/kernel/drivers/bus/pci_bus.cpp"
  "${OS1_REPO_ROOT}/src/kernel/drivers/usb/hid_keyboard.cpp"
  "${OS1_REPO_ROOT}/src/kernel/drivers/usb/xhci_controller.cpp"
  "${OS1_REPO_ROOT}/src/kernel/fs/cpio_newc.cpp"
  "${OS1_REPO_ROOT}/src/kernel/handoff/boot_info.cpp"
  "${OS1_REPO_ROOT}/src/kernel/mm/boot_mapping.cpp"
  "${OS1_REPO_ROOT}/src/kernel/drivers/bus/resource.cpp"
  "${OS1_REPO_ROOT}/src/kernel/mm/page_frame.cpp"
  "${OS1_REPO_ROOT}/src/kernel/mm/dma.cpp"
  "${OS1_REPO_ROOT}/src/kernel/mm/virtual_memory.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/acpi.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/hpet.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/irq_registry.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/pci_capability.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/pci_config.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/pci_msi.cpp"
  "${OS1_REPO_ROOT}/src/kernel/platform/state.cpp"
  "${OS1_REPO_ROOT}/src/kernel/proc/user_elf.cpp"
)
target_compile_definitions(os1_host_support PUBLIC OS1_HOST_TEST=1)
target_include_directories(os1_host_support PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}"
  "${OS1_REPO_ROOT}/src/common"
  "${OS1_REPO_ROOT}/src/kernel"
  "${OS1_REPO_ROOT}/src/uapi"
  "${OS1_HOST_GENERATED_DIR}"
)

add_executable(os1_host_tests
  common/elf64_tests.cpp
  common/freestanding_string_tests.cpp
  fs/cpio_newc_tests.cpp
  handoff/boot_info_tests.cpp
  kernel/acpi_hpet_tests.cpp
  kernel/utility_tests.cpp
  kernel/event_ring_tests.cpp
  kernel/irq_route_registry_tests.cpp
  kernel/irq_vector_allocator_tests.cpp
  kernel/timer_source_tests.cpp
  kernel/pci_bus_tests.cpp
  kernel/pci_capability_tests.cpp
  kernel/pci_msi_tests.cpp
  kernel/pci_resource_tests.cpp
  kernel/hid_keyboard_tests.cpp
  kernel/xhci_controller_tests.cpp
  mm/page_frame_tests.cpp
  mm/dma_tests.cpp
  mm/user_address_tests.cpp
  mm/virtual_memory_tests.cpp
  proc/user_elf_tests.cpp
  syscall/observe_abi_tests.cpp
)
target_link_libraries(os1_host_tests PRIVATE os1_host_support GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(os1_host_tests
  DISCOVERY_TIMEOUT 30
  PROPERTIES LABELS "host;unit"
)
