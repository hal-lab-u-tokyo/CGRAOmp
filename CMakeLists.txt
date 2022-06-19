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
#    File:          /CMakeLists.txt
#    Project:       CGRAOmp
#    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
#    Created Date:  26-08-2021 20:50:56
#    Last Modified: 19-06-2022 19:52:21
#

# See docs/CMake.html for instructions about how to build LLVM with CMake.

cmake_minimum_required(VERSION 3.4.3)

project(CGRAOmp VERSION 0.1)

if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
  set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
endif()

if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif()

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

set (CGRAOMP_ENABLE_DOXYGEN OFF CACHE BOOL "Use doxygen to generate llvm API documentation.")
set(CGRAOMP_INSTALL_DOXYGEN_HTML_DIR "share/docs/doxygen-html"
    CACHE STRING "Doxygen-generated HTML documentation install directory")

add_subdirectory(src)

# copy some resources to build directory
file(COPY share DESTINATION ${CMAKE_BINARY_DIR})

# installing docs, etc
install(DIRECTORY share/ DESTINATION share)

# installing compiler driver
install(FILES scripts/cgraomp-cc
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ WORLD_READ
        DESTINATION bin/)
install(DIRECTORY scripts/cc_config DESTINATION lib/)
install(DIRECTORY scripts/backend DESTINATION lib/)