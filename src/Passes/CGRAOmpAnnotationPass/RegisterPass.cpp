/*
*    MIT License
*    
*    Copyright (c) 2021 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
*    
*    Permission is hereby granted, free of charge, to any person obtaining a copy of
*    this software and associated documentation files (the "Software"), to deal in
*    the Software without restriction, including without limitation the rights to
*    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*    of the Software, and to permit persons to whom the Software is furnished to do
*    so, subject to the following conditions:
*    
*    The above copyright notice and this permission notice shall be included in all
*    copies or substantial portions of the Software.
*    
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*    SOFTWARE.
*    
*    File:          /src/Passes/CGRAOmpAnnotationPass/RegisterPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  14-12-2021 13:36:47
*    Last Modified: 14-12-2021 14:36:22
*/
#include "llvm/Passes/PassBuilder.h"

#include "CGRAOmpAnnotationPass.hpp"


using namespace CGRAOmp;

// callback function to register passes

static void registerModuleAnalyses(ModuleAnalysisManager &MAM)
{
#define MODULE_ANALYSIS(CREATE_PASS) \
	MAM.registerPass([&] { \
		return CREATE_PASS; });

#include "AnnotationPasses.def"

}

static void registerFunctionAnalyses(FunctionAnalysisManager &FAM)
{
#define FUNCTION_ANALYSIS(CREATE_PASS) \
	FAM.registerPass([&] { return CREATE_PASS; });

#include "AnnotationPasses.def"

}

static void registerAnnotationPasses(PassBuilder &PB)
{
	PB.registerAnalysisRegistrationCallback(registerModuleAnalyses);
	PB.registerAnalysisRegistrationCallback(registerFunctionAnalyses);
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "CGRAOmp", LLVM_VERSION_STRING, registerAnnotationPasses};
}

