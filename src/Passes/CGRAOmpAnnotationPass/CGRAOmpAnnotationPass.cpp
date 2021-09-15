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
*    File:          /src/Passes/CGRAOmpAnnotationPass/CGRAOmpAnnotationPass.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  15-09-2021 09:52:46
*    Last Modified: 15-09-2021 11:30:07
*/

#include "CGRAOmpAnnotationPass.hpp"

using namespace llvm;
using namespace CGRAOmp;

AnalysisKey AnnotationAnalysisPass::Key;
AnalysisKey ModuleAnnotationAnalysisPass::Key;

ModuleAnnotationAnalysisPass::Result
ModuleAnnotationAnalysisPass::run(Module &M, ModuleAnalysisManager &AM)
{
	Result result;
	if (auto I = M.getGlobalVariable(LLVM_ANNOT_NAME)) {
		auto *CArr = dyn_cast<ConstantArray>(I->getOperand(0));
		if (!CArr) return result;
		for (auto &U : CArr->operands()) {
			auto *CS = dyn_cast<ConstantStruct>(U.get());
			if (!CS) continue;
			auto *f = dyn_cast<Function>(CS->getOperand(0)->getOperand(0));
			if (!f) continue;
			auto *anno_var = dyn_cast<GlobalVariable>(CS->getOperand(1)->getOperand(0));
			if (!anno_var) continue;
			auto anno_str = dyn_cast<ConstantDataArray>(anno_var->getInitializer())->getAsCString();
			result[f].insert(anno_str);
		}
	}
	return result;
}

/**
 * @details At the beginning, it checks if the cached result of ModuleAnnotationAnalysisPass is available. If not, it finds annotations of the function.
 *
*/
AnnotationAnalysisPass::Result
AnnotationAnalysisPass::run(Function &F, FunctionAnalysisManager &AM)
{
	// try to use cached result
	auto &MAMProxy = AM.getResult<ModuleAnalysisManagerFunctionProxy>(F);
	auto M = F.getParent();
	auto *modResult = MAMProxy.getCachedResult<ModuleAnnotationAnalysisPass>(*M);
	if (modResult) {
		auto anno_set = modResult->find(&F);
		if (anno_set != modResult->end()) {
			return anno_set->second;
		}
	}

	Result result;
	if (auto I = M->getGlobalVariable(LLVM_ANNOT_NAME)) {
		auto *CArr = dyn_cast<ConstantArray>(I->getOperand(0));
		if (!CArr) return result;
		for (auto &U : CArr->operands()) {
			auto *CS = dyn_cast<ConstantStruct>(U.get());
			if (!CS) continue;
			auto *f = dyn_cast<Function>(CS->getOperand(0)->getOperand(0));
			if (!f) continue;
			if (f != &F) continue;
			auto *anno_var = dyn_cast<GlobalVariable>(CS->getOperand(1)->getOperand(0));
			if (!anno_var) continue;
			auto anno_str = dyn_cast<ConstantDataArray>(anno_var->getInitializer())->getAsCString();
			result.insert(anno_str);
		}
	}
	return result;
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION, "CGRAOmp", "v0.1",
		[](PassBuilder &PB) {
			PB.registerAnalysisRegistrationCallback(
				[](ModuleAnalysisManager &MAM) {
					MAM.registerPass([&] {
						return ModuleAnnotationAnalysisPass();
					});
			});
			PB.registerAnalysisRegistrationCallback(
				[](FunctionAnalysisManager &FAM) {
					FAM.registerPass([&] {
						return AnnotationAnalysisPass();
					});
			});
		}
	};
}
