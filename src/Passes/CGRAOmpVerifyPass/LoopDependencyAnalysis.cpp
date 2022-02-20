/*
*    MIT License
*    
*    Copyright (c) 2022 Amano laboratory, Keio University & Processor Research Team, RIKEN Center for Computational Science
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
*    File:          /src/Passes/CGRAOmpVerifyPass/LoopDependencyAnalysis.cpp
*    Project:       CGRAOmp
*    Author:        Takuya Kojima in The University of Tokyo (tkojima@hal.ipc.i.u-tokyo.ac.jp)
*    Created Date:  18-02-2022 18:14:28
*    Last Modified: 20-02-2022 22:37:54
*/
#include "LoopDependencyAnalysis.hpp"
#include "OptionPlugin.hpp"
#include "common.hpp"

#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/IVDescriptors.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"
static const char *VerboseDebug = DEBUG_TYPE "-verbose";

/* ================= Implementation of LoopCarriedDependence ================= */
AnalysisKey LoopDependencyAnalysisPass::Key;

LoopDependencyAnalysisPass::Result LoopDependencyAnalysisPass::run(Loop &L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR)
{
	Result result;

	SmallPtrSet<PHINode*, 32> indvar_set;
	SmallPtrSet<PHINode*, 32> datadep;

	// check data dependency
	SmallPtrSet<BasicBlock*, 32> all_blocks(L.getBlocks().begin(), L.getBlocks().end());

	// obtain loop induction variable
	auto LN = LoopNest::getLoopNest(L, AR.SE);
	for (auto *nest : LN->getLoops()) {
		InductionDescriptor IDV;
		if (nest->getInductionDescriptor(AR.SE, IDV)) {
			auto start = IDV.getStartValue();
			auto carried = IDV.getInductionBinOp();
			auto indvar = nest->getInductionVariable(AR.SE);
			if (auto step = IDV.getConstIntStepValue()) {
				indvar_set.insert(indvar);
				auto IVDep = new InductionVariableDependency(indvar, carried, start, step);
				result.add_idv_dep(IVDep);
			} else {
				// not constant step
			}
		}
	}

	// obtain loop carried dependency via register
	for (auto BB : all_blocks) {
		// check for each phi
		for (auto &phi : BB->phis()) {
			if (!indvar_set.contains(&phi)) {
				datadep.insert(&phi);
				Value *init = nullptr;
				Instruction *carried = nullptr;
				for (int i = 0; i < phi.getNumIncomingValues(); i++) {
					auto in_val = phi.getIncomingValue(i);
					auto in_BB = phi.getIncomingBlock(i);
					if (all_blocks.contains(in_BB)) {
						if (carried) {
							// the other in-coming edge already visited is from the loop 
							break;
						}
						carried = dyn_cast<Instruction>(in_val);
					} else {
						assert(!init && "multiple in-comming edges from outside of the loop");
						init = in_val;
					}
				}
				if (init && carried) {
					// found loop-carried dependency
					auto SDep = new SimpleLoopDependency(carried, init, &phi);
					result.add_dep(SDep);
				}
			}
		}
	}

	// check data dependency via memory
	// considering only RAW hazard
	auto &LAI = AM.getResult<LoopAccessAnalysis>(L, AR);
	auto checker = LAI.getDepChecker();
	auto deps = checker.getDependences();
	for (auto dep : *deps) { 
		if (dep.isBackward()) {
			LoadInst *use = dyn_cast<LoadInst>(dep.getSource(LAI));
			StoreInst *def = dyn_cast<StoreInst>(dep.getDestination(LAI));
			
			if (!use || !def) continue; //not store->load

			auto Dist = getDistance(def, use, AR.SE);
			if (Dist.hasValue()) {
				if (*Dist <= OptMemoryDependencyDistanceThreshold) {
					// treat it as data dependency
					auto MDep = new MemoryLoopDependency(def, use, *Dist);
					result.add_mem_dep(MDep);
				}
			} else {
				// cannot compute distance
				LLVM_DEBUG(
					dbgs() << WARN_DEBUG_PREFIX << "cannot compute dependence distance between\n";
					def->print(dbgs() << "\t");
					dbgs() << "\tand\n";
					use->print(dbgs());
					dbgs() << "\n";
				);
			}
		}
	}

	return result;
}

Optional<int> LoopDependencyAnalysisPass::getDistance(Instruction *A, Instruction *B, ScalarEvolution &SE)
{
	Value *APtr = nullptr, *BPtr = nullptr;
	int word_size_a, word_size_b;
	if (isa<LoadInst>(*A)) {
		APtr = A->getOperand(0);
		word_size_a = A->getType()->getPrimitiveSizeInBits() / 8;
	} else if (isa<StoreInst>(*A)) {
		APtr = A->getOperand(1);
		word_size_a = A->getOperand(0)->getType()->getPrimitiveSizeInBits() / 8;
	}
	if (isa<LoadInst>(*B)) {
		BPtr = B->getOperand(0);
		word_size_b = B->getType()->getPrimitiveSizeInBits() / 8;
	} else if (isa<StoreInst>(*B)) {
		BPtr = B->getOperand(1);
		word_size_b = B->getOperand(0)->getType()->getPrimitiveSizeInBits() / 8;
	}

	if (APtr && BPtr) {
		assert(word_size_a == word_size_b && "both data size must be the same");

		const SCEV *APtr_scev = SE.getSCEV(APtr);
		const SCEV *BPtr_scev = SE.getSCEV(BPtr);
		
		if (auto SC = dyn_cast<SCEVConstant>(SE.getMinusSCEV(APtr_scev, BPtr_scev))) {
			if (auto dist = SC->getAPInt().getRawData()) {
				return *dist / word_size_a;
			}
		}
	}

	return None;


}