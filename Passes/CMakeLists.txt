# set LLVM version
if(NOT DEFINED LLVM_VERSION_MAJOR)
  set(LLVM_VERSION_MAJOR 12)
endif()
if(NOT DEFINED LLVM_VERSION_MINOR)
  set(LLVM_VERSION_MINOR 0)
endif()
if(NOT DEFINED LLVM_VERSION_PATCH)
  set(LLVM_VERSION_PATCH 0)
endif()
if(NOT DEFINED LLVM_VERSION_SUFFIX)
  set(LLVM_VERSION_SUFFIX "")
endif()

# get LLVM cmake dir
set (LLVM_MIN_VERSION "${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH}")
execute_process(COMMAND llvm-config --cmakedir OUTPUT_VARIABLE LLVM_DIR)

find_package(LLVM ${LLVM_MIN_VERSION} REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# list of passes to be built
set (LLVM_PASSLIB_LIST
)

message("${LLVM_PASSLIB_LIST}")
# add for each lib
foreach (PASSLIB IN LISTS LLVM_PASSLIB_LIST)
  add_subdirectory(${PASSLIB})
  message("add target library: ${PASSLIB}")
endforeach()
