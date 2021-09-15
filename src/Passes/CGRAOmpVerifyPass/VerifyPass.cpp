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
*    File:          /src/Passes/CGRAOmpVerifyPass/VerifyPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  27-08-2021 15:03:52
*    Last Modified: 15-09-2021 11:18:39
*/
#include "VerifyPass.hpp"
#include "CGRAOmpAnnotationPass.hpp"

using namespace llvm;
using namespace CGRAOmp;

AnalysisKey VerifyPass::Key;

void VerifyResult::print(raw_ostream &OS) const
{
	OS << "this is verifyResult\n";
}

bool VerifyResult::bool_operator_impl()
{
	isViolate = false;
	// if at least one verification result is violation, it returns violation
	for (auto it : make_range(each_result.begin(), each_result.end())) {
		if (!*(it.second)) {
			isViolate = true;
			break;
		}
	}
	return !isViolate;
}


VerifyResult VerifyPass::run(Function &F, FunctionAnalysisManager &AM)
{
	errs() << "verifying " << F.getName() << "\n";
	VerifyResult result;

	GET_MODEL_FROM_FUNCTION(model);

	for (auto &BB : F) {
		for (auto &I : BB) {
			if (auto entry = model->isSupported(&I)) {
				I.dump();
				entry->dump();
			}
		}
	}
	return result;
}

PreservedAnalyses VerifyModulePass::run(Module &M, ModuleAnalysisManager &AM)
{
	errs() << "verification\n";

	auto &MM = AM.getResult<ModelManagerPass>(M);
	auto model = MM.getModel();

	// verify all OpenMP kernel
	for (auto &F : M) {
		// skik only declaration
		if (F.isDeclaration()) continue;
		// verify OpenMP target function
		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		auto verify_res = FAM.getResult<VerifyPass>(F);
	}

	// there is no modification so it does not keep all analysis
	return PreservedAnalyses::all();
}


extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", "v0.1",
		[](PassBuilder &PB) {
			PB.registerAnalysisRegistrationCallback(
				[](FunctionAnalysisManager &FAM) {
					FAM.registerPass([&] {
						return VerifyPass();
					});
			});
		}
	};
}