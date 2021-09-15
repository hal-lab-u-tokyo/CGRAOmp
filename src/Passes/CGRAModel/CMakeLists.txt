add_llvm_library( libCGRAModel MODULE
  ## append source file list here
  CGRAModel.cpp
  CGRAInstMap.cpp
  
  DEPENDS
  intrinsics_gen

  PLUGIN_TOOL
  opt
  )

target_include_directories( libCGRAModel
  PRIVATE ${PROJECT_SOURCE_DIR}/include
  )
