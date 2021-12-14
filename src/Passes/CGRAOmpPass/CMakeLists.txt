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
#    File:          /src/Passes/CGRAOmpPass/CMakeLists.txt
#    Project:       CGRAOmp
#    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
#    Created Date:  27-08-2021 14:16:34
#    Last Modified: 14-12-2021 18:51:01
#
add_llvm_library( libCGRAOmpPass MODULE
  ## append source file list here
  CGRAOmpPass.cpp
  OmpPasses.def

  DEPENDS
  intrinsics_gen

  PLUGIN_TOOL
  opt
  )

target_include_directories( libCGRAOmpPass
  PRIVATE ${PROJECT_SOURCE_DIR}/include
)

