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
*    File:          /src/Passes/CGRAOmpVerifyPass/DecoupledAnalysis.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in Amano Laboratory, Keio University (tkojima@am.ics.keio.ac.jp)
*    Created Date:  14-12-2021 11:38:23
*    Last Modified: 14-12-2021 15:34:47
*/

#include "llvm/Analysis/LoopNestAnalysis.h"

#include "DecoupledAnalysis.hpp"
#include "CGRAOmpPass.hpp"

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

/* ================= Implementation of DecoupledAnalysis ================= */

/* ================= Implementation of DecoupledAnalysisPass ================= */
AnalysisKey DecoupledAnalysisPass::Key;

DecoupledAnalysisPass::Result DecoupledAnalysisPass::run(Loop &L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR)
{
	Result result;
	// get cached result
	auto &FAMProxy = AM.getResult<FunctionAnalysisManagerLoopProxy>(L, AR);
	auto *F = (*(L.block_begin()))->getParent();
	auto SI = FAMProxy.getCachedResult<OmpStaticShecudleAnalysis>(*F);
	assert(SI && "OmpStaticScheduleAnalysis must be executed before any loop pass");

	// get innermost loop
	auto LN = LoopNest::getLoopNest(L, AR.SE);
	auto innermost = LN->getInnermostLoop();
	auto preheader = innermost->getLoopPreheader();
	assert(innermost && "Innermost loop is not found");

	SmallVector<LoadInst*> mem_load;
	SmallVector<StoreInst*> mem_store;

	// search for memory access for computation
	for (auto &BB : innermost->getBlocks()) {
		for (auto &I : *BB) {
			if (auto ld = dyn_cast<LoadInst>(&I)) {
				if (!SI->contains(ld->getOperand(0)) && !isPointerValue(ld)) {
					mem_load.push_back(ld);
				} // otherwise, it is an information about loop scheduling
				  // thus, it must not be treated as input data for data flow
			} else if (auto st = dyn_cast<StoreInst>(&I)) {
				mem_store.push_back(st);
			}
		}
	}

	//save the momory access insts
	result.setMemLoad(std::move(mem_load));
	result.setMemStore(std::move(mem_store));
	return result;
}

bool DecoupledAnalysisPass::isPointerValue(LoadInst *I)
{
	auto operandTy = I->getOperand(0)->getType();
	if (operandTy->isPointerTy()) {
		// if true it is pointer of pointer
		return operandTy->getPointerElementType()->isPointerTy();
	}
	return false;
}

