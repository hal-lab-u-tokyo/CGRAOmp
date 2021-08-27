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
*    File:          /Passes/CGRAOmpPass/CGRAOmpPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:19:22
*    Last Modified: 27-08-2021 17:11:20
*/
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"

#include "llvm/IR/Function.h"

using namespace llvm;
using namespace CGRAOmp;

PreservedAnalyses CGRAOmpPass::run(Module &M, ModuleAnalysisManager &AM)
{
	errs() << "CGRAOmpPass is called\n";
	for (auto &F : M) {
		errs() << F.getName() << "\n";
		// Skip declaration
		if (F.isDeclaration()) continue;

		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		errs() << "get FAM\n";
		auto verify_res = FAM.getResult<VerifyPass>(F);
		errs() << "get Rest\n";
		errs() << verify_res << "\n";
	}
	return PreservedAnalyses::all();
}


extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", "v0.1",
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, ModulePassManager &PM,
					ArrayRef<PassBuilder::PipelineElement>){
						if(Name == "cgraomp"){
							PM.addPass(CGRAOmpPass());
							return true;
						}
						return false;
				}
			);
		}
	};
}