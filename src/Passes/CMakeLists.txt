#
#    MIT License
#    
#    Copyright (c) 2021 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
#    
#    Permission is hereby granted, free of charge, to any person obtaining a copy of
#    this software and associated documentation files (the "Software"), to deal in
#    the Software without restriction, including without limitation the rights to
#    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
#    of the Software, and to permit persons to whom the Software is furnished to do
#    so, subject to the following conditions:
#    
#    The above copyright notice and this permission notice shall be included in all
#    copies or substantial portions of the Software.
#    
#    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#    SOFTWARE.
#    
#    File:          /src/Passes/CMakeLists.txt
#    Project:       CGRAOmp
#    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
#    Created Date:  26-08-2021 20:53:10
#    Last Modified: 01-02-2022 19:59:32
#

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
# Polly is needed
find_package(Polly REQUIRED CONFIG)
message(STATUS "Found Polly")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS} ${Polly_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# list of passes to be built
set (LLVM_PASSLIB_LIST
  CGRAOmpComponents
  CGRAOmpPass
  CGRAOmpVerifyPass
  CGRAOmpAnnotationPass
  CGRAModel
  CGRAOmpDFGPass
  HelloDFGPass
)

# add for each lib
message("-- LLVM libraries to be built for CGRAOmp")
foreach (PASSLIB IN LISTS LLVM_PASSLIB_LIST)
  add_subdirectory(${PASSLIB})
  message(" add target library: ${PASSLIB}")
endforeach()


