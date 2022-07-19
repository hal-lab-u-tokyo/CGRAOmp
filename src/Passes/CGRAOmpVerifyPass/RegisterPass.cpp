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
*    File:          /src/Passes/CGRAOmpVerifyPass/RegisterPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  14-12-2021 12:39:40
*    Last Modified: 18-02-2022 18:46:09
*/
#include "llvm/Passes/PassBuilder.h"

#include "VerifyPass.hpp"
#include "DecoupledAnalysis.hpp"
#include "AGVerifyPass.hpp"
#include "LoopDependencyAnalysis.hpp"

using namespace CGRAOmp;

// callback function to register passes

static void registerFunctionAnalyses(FunctionAnalysisManager &FAM)
{
#define FUNCTION_ANALYSIS(CREATE_PASS) \
	FAM.registerPass([&] { return CREATE_PASS; });

#include "VerifyPasses.def"

}

static void registerLoopAnalyses(LoopAnalysisManager &LAM)
{
#define LOOP_ANALYSIS(CREATE_PASS) \
	LAM.registerPass([&] { return CREATE_PASS; });

#include "VerifyPasses.def"

}

static void registerVerifyPasses(PassBuilder &PB)
{
	PB.registerAnalysisRegistrationCallback(registerFunctionAnalyses);
	PB.registerAnalysisRegistrationCallback(registerLoopAnalyses);
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {LLVM_PLUGIN_API_VERSION, "CGRAOmp", LLVM_VERSION_STRING, registerVerifyPasses};
}