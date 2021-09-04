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
#    File:          /src/CMakeLists.txt
#    Project:       CGRAOmp
#    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
#    Created Date:  04-09-2021 17:17:57
#    Last Modified: 04-09-2021 17:23:32
#
if (CGRAOMP_ENABLE_DOXGEN)
  find_package(Doxygen)
  IF(DOXYGEN_FOUND)
    message("-- Enables building doxygen")
    set(DOXYGEN_CONF_FILE "doxygen.conf")
    set(DOXYGEN_CONF_PATH   ${CMAKE_CURRENT_BINARY_DIR}/${DOXYGEN_CONF_FILE})
    set(DOXYGEN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYGEN_TARGET "doc" )

    configure_file(${DOXYGEN_CONF_FILE}.in ${DOXYGEN_CONF_PATH})

    add_custom_target(${DOXYGEN_TARGET} ALL
                    ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONF_PATH}
                    DEPENDS  ${DOXYGEN_CONF_PATH})
    install(DIRECTORY ${CMAKE_BINARY_DIR}/${CGRAOMP_INSTALL_DOXYGEN_HTML_DIR}/ 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/${CGRAOMP_INSTALL_DOXYGEN_HTML_DIR})
  else (DOXYGEN_FOUND)
    message (WARNING "building doxygen option is specified but doxygen is not found")
  endif (DOXYGEN_FOUND)
endif()

# LLVM Pass libraries
add_subdirectory(Passes)
