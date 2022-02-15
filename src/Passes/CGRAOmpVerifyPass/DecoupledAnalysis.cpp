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
*    Last Modified: 15-02-2022 16:34:28
*/

#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/ADT/SetOperations.h"

#include "DecoupledAnalysis.hpp"
#include "CGRAOmpPass.hpp"

#include <queue>

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"
static const char *VerboseDebug = DEBUG_TYPE "-verbose";


/* ================= Implementation of DecoupledAnalysisPass ================= */
AnalysisKey DecoupledAnalysisPass::Key;

/**
 * @details 
 * Analyzed Loop @em L must be perfectly nested, i.e., memory access exist only in the inner most loop.
 *
*/
DecoupledAnalysisPass::Result DecoupledAnalysisPass::run(Loop &L, LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX 
				<< "Start decoupling process for "
					<< L.getName() << "\n");
	Result result;
	// get cached result
	auto &FAMProxy = AM.getResult<FunctionAnalysisManagerLoopProxy>(L, AR);
	auto *F = (*(L.block_begin()))->getParent();
	auto SI = FAMProxy.getCachedResult<OmpStaticShecudleAnalysis>(*F);
	assert(SI && "OmpStaticScheduleAnalysis must be executed before any loop pass");

	// get innermost loop
	auto LN = LoopNest::getLoopNest(L, AR.SE);
	auto innermost = LN->getInnermostLoop();
	assert(innermost && "Innermost loop is not found");

	SmallPtrSet<LoadInst*, 32> mem_load;
	SmallPtrSet<StoreInst*, 32> mem_store;
	SmallPtrSet<User*, 32> kernel_nodes;

	// search for memory access for computation
	for (auto &BB : innermost->getBlocks()) {
		for (auto &I : *BB) {
			kernel_nodes.insert(&I);
			if (auto ld = dyn_cast<LoadInst>(&I)) {
				if (!SI->contains(ld->getOperand(0)) && !isPointerValue(ld)) {
					mem_load.insert(ld);
				} // otherwise, it is an information about loop scheduling
				  // thus, it must not be treated as input data for data flow
			} else if (auto st = dyn_cast<StoreInst>(&I)) {
				mem_store.insert(st);
			}
		}
	}

	// fifo for breath first search
	std::queue<User*> fifo;
	SmallPtrSet<User*, 32> traversed;

	//push all mem load inst
	for (User *v : mem_load) {
		fifo.push(v);
	}
	
	//breadth first search from memory load to store
	while (!fifo.empty()) {
		User *v = fifo.front();
		fifo.pop();
		// store means the end of data-flow so not traverse any more
		if (!mem_store.contains(dyn_cast<StoreInst>(v))) {
			// queue all successors
			for (auto *suc : v->users()) {
				if (isa<LoadInst>(*suc)) {
					// impossible to decouple
					result.setError("Loop dependent mem loads are included");
					return result;
				}
				if (!traversed.contains(suc)) {
					fifo.push(suc);
					traversed.insert(suc);
				}
			}
		}
	}

	SmallPtrSet<User*, 32> reached(traversed);
	// for set operations
	SmallPtrSet<User*, 32> stores(mem_store.begin(), mem_store.end());

	set_intersect(reached, stores);
	set_subtract(traversed, stores);

	// check all memory stores are reachable
	if (reached.size() < stores.size()) {
		result.setError("Unreachable store exists");
		return result;
	}

	// a set of decoupled computation node
	SmallVector<User*> comp(traversed.begin(), traversed.end());
	SmallVector<Value*> invars;

	// tracking in-comming edge
	for (auto *user : traversed) {
		int last_operand = user->getNumOperands();
		// the last operand of store is destination
		// the last operand of callinst is function, so skip it for tracking
		if (isa<StoreInst>(*user) || isa<CallInst>(*user)) {
			last_operand--;
		}

		for (int i = 0; i < last_operand; i++) {
			if (auto operand = dyn_cast<User>(user->getOperand(i))) {
				// check if it is first looked node
				if (!traversed.contains(operand) &&
						!mem_load.contains(dyn_cast<LoadInst>(operand))) {
					// constant values
					if (auto *c = dyn_cast<Constant>(operand)) {
						invars.emplace_back(c);
					} else {
						// skip some node
						Value* last = operand;
						SmallVector<Value*> hist;
						while (isa<TruncInst>(*last) || isa<BitCastInst>(*last)) {
							if (auto next = dyn_cast<User>(last)->getOperand(0)) {
								hist.emplace_back(last);
								last = next;
							} else {
								break;
							}
						}
						// check if it is defined outside the loop
						if (!kernel_nodes.contains(dyn_cast<User>(last))) {
							invars.emplace_back(last);
							// save skip history
							if (hist.size() > 0) {
								result.setInvarSkipHistory(last, hist);
							}
						} else {
							LLVM_DEBUG(
								dbgs() << WARN_DEBUG_PREFIX  << "Unreachable nodes inside the kernel: ";
								last->print(dbgs());
								dbgs() << "\n";
							);
						}
					}
				}
			} else {
				// not user type
				DEBUG_WITH_TYPE(VerboseDebug,
					dbgs() << DBG_DEBUG_PREFIX << "Source node of ";
					user->print(dbgs());
					dbgs() << " is not User type";
				);
			}
		}
	}

	//save the momory access insts
	result.setMemLoad(DecoupledAnalysis::MemLoadList(mem_load.begin(), mem_load.end()));
	result.setMemStore(DecoupledAnalysis::MemStoreList(mem_store.begin(), mem_store.end()));
	result.setComp(std::move(comp));
	result.setInvars(std::move(invars));
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

