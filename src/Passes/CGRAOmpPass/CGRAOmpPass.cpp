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
*    File:          /src/Passes/CGRAOmpPass/CGRAOmpPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 14:19:22
*    Last Modified: 15-09-2021 11:17:47
*/
#include "CGRAOmpPass.hpp"
#include "VerifyPass.hpp"
#include "CGRAModel.hpp"
#include "OptionPlugin.hpp"
#include "CGRAOmpAnnotationPass.hpp"

#include "llvm/ADT/Statistic.h"


using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

// # of successfully exracted DFGs
STATISTIC(num_dfg, "the number of extracted DFGs");

AnalysisKey ModelManagerPass::Key;

ModelManagerPass::Result
ModelManagerPass::run(Module &M, ModuleAnalysisManager &AM)
{
	// for cache result
	AM.getResult<ModuleAnnotationAnalysisPass>(M);

	errs() << "Instantiate CGRAModel\n";
	auto ErrorOrModel = parseCGRASetting(PathToCGRAConfig);
	if (!ErrorOrModel) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		Exit(std::move(ErrorOrModel.takeError()));
	}
	auto model = *ErrorOrModel;

	return ModelManager(model);
}

bool ModelManager::invalidate(Module& M, const PreservedAnalyses &PA,
								ModuleAnalysisManager::Invalidator &Inv)
{
	// always keeping this reuslt valid after creation
	auto PAC = PA.getChecker<ModelManagerPass>();
	return !PAC.preservedWhenStateless();
}

#undef DEBUG_TYPE

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", "v0.1",
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, ModulePassManager &PM,
					ArrayRef<PassBuilder::PipelineElement>){
						if (Name == "cgraomp") {
							// make a pipeline
							// Verify->DFGExraction->Runtime Insertion
							PM.addPass(VerifyModulePass());
							return true;
						}
						return false;
				}
			);

			PB.registerAnalysisRegistrationCallback(
				[](ModuleAnalysisManager &MAM) {
					MAM.registerPass([&] {
						return ModelManagerPass();
					});
			});
		}
	};
}
