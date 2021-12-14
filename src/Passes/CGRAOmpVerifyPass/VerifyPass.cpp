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
*    Last Modified: 14-12-2021 15:47:30
*/

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Support/Error.h"

#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"

#include "VerifyPass.hpp"
#include "CGRAOmpAnnotationPass.hpp"
#include "DecoupledAnalysis.hpp"

#include <system_error>

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

static const char *VerboseDebug = DEBUG_TYPE "-verbose";


/* ===================== Implementation of VerifyResult ===================== */

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

/* ================== Implementation of GenericVerifyPass ================== */
AnalysisKey GenericVerifyPass::Key;

VerifyResult GenericVerifyPass::run(Function &F, FunctionAnalysisManager &AM)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Verifying "
				 << F.getName() << "for generic CGRA\n");
	VerifyResult result;

	GET_MODEL_FROM_FUNCTION(model);

	assert(!"GenericVerifyPass is not implemented");

	return result;
}

/* ================= Implementation of DecoupledVerifyPass ================= */
AnalysisKey DecoupledVerifyPass::Key;

VerifyResult DecoupledVerifyPass::run(Function &F, FunctionAnalysisManager &AM)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Verifying a kernel for decoupled CGRA: "
				 << F.getName() << "\n");
	VerifyResult result;

	GET_MODEL_FROM_FUNCTION(model);
	auto dec_model = model->asDerived<DecoupledCGRA>();

	// ensure OmpStaticShecudleAnalysis result is cached for DecoupledAnalysis
	auto SI = AM.getResult<OmpStaticShecudleAnalysis>(F);
	if (!SI) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		std::error_code EC;
		Exit(make_error<StringError>("Fail to find OpenMP scheduling info", EC));
	}

	switch (dec_model->getAG()->getKind()) {
		case AddressGenerator::Kind::Affine:
			{ auto ag_comapt = AM.getResult<VerifyAffineAGCompatiblePass>(F); }
			break;
		case AddressGenerator::Kind::FullState:
			assert(!"FullState AG is not implemented yet");
			break;
	}

	return result;
}

/* ================= Implementation of VerifyPassBase ================= */
/**
 * @details 
 * This routine finds maximum perfectly nested loops in the function
 * The followings are some examples:
 * @code
 * for (...) {
 *   for (...) { // <- found as the perfect nested loop
 *     for (...) {
 *        ....
 *     }
 *   }
 *   some statements...
 * }
 * @endcode
*/
template <typename DerivedT>
SmallVector<Loop*> VerifyPassBase<DerivedT>::findPerfectlyNestedLoop(Function &F,
										 LoopStandardAnalysisResults &AR)
{
	SmallVector<Loop*> loop_kernels;
	for (Loop *outerLoop : AR.LI) {
		DEBUG_WITH_TYPE(VerboseDebug, dbgs() << DBG_DEBUG_PREFIX
						<< "Analyzing loop nest structure of " <<
						outerLoop->getName() << "\n");
		// get LoopNest analysis
		auto LN = LoopNest::getLoopNest(*outerLoop, AR.SE);
		// int depth = LN->getNestDepth();

		//find maximum perfectly nested loops from the innermost to the outermost
		if (auto innermost = LN->getInnermostLoop()) {
			for (Loop *inL : LN->getLoops()) {
				auto inLN = LoopNest::getLoopNest(*inL, AR.SE);
				int max_nest = inLN->getMaxPerfectDepth();

				if (inLN->getNestDepth() == inLN->getMaxPerfectDepth()) {
					LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << 
						"Detected perfectly nested loop: " << inL->getName() << 
						" Nested level " << max_nest << "\n");
					loop_kernels.push_back(inL);
					break;
				}
			}
		} else {
			LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX
						<< "detect multiple innermost loops\n");
		}
	}

	return std::move(loop_kernels);
}

/* ============= Implementation of VerifyAffineAGCompatiblePass ============= */
AnalysisKey VerifyAffineAGCompatiblePass::Key;

AffineAGCompatibility VerifyAffineAGCompatiblePass::run(Function &F,
											FunctionAnalysisManager &AM)
{
	Result result;
	GET_MODEL_FROM_FUNCTION(model);
	auto dec_model = model->asDerived<DecoupledCGRA>();

	// get outer most loops
	auto AR = getLSAR(F, AM);
	auto &LPM = AM.getResult<LoopAnalysisManagerFunctionProxy>(F).getManager();

	auto loop_kernels = findPerfectlyNestedLoop(F, AR);

	if (loop_kernels.size() == 0) {
		LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX << "Cannot find any valid loop kernels\n");
		return result;
	}

	// verify each memory access
	for (auto L : loop_kernels) {
		LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX 
					<< "Verifying Affine AG compatibility of a loop: "
					<< L->getName() << "\n");
		auto DA = LPM.getResult<DecoupledAnalysisPass>(*L, AR);

		verify_affine_access(*L, LPM, AR, result);
	}

	return result;
}


void VerifyAffineAGCompatiblePass::verify_affine_access(Loop &L,
								LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR, Result &R)
{
	auto DA = AM.getResult<DecoupledAnalysisPass>(L, AR);

	// //for each outermost loop
	// for (Loop *l : AR.LI) {
	// 	if (l->getLoopDepth() == 1) {
	// 		auto LN = LoopNest::getLoopNest(*l, AR.SE);
	// 		si = AM.getResult<OmpStaticShecudleAnalysis>(*l, AR);
	// 		if (si) {
	// 			// get valid schedule info
	// 		} else {
	// 			// fail to get schedule info
	// 			LLVM_DEBUG(dbgs() << ERR_DEBUG_PREFIX << "Fail to get scheduling info");
	// 		}
	// 	}
	// }
	// auto LN = LoopNest::getLoopNest(L, AR.SE);
	// auto innermost = LN->getInnermostLoop();
	// auto preheader = innermost->getLoopPreheader();
	// assert(innermost && "Innermost loop is not found");

	// SmallVector<LoadInst*> mem_load;
	// SmallVector<StoreInst*> mem_store;

	// auto& AA = AR.AA;

	// // search for memory access for computation
	// for (auto &BB : innermost->getBlocks()) {
	// 	for (auto &I : *BB) {
	// 		if (auto ld = dyn_cast<LoadInst>(&I)) {
	// 			if (!si.contains(ld->getOperand(0))) {
	// 				mem_load.push_back(ld);
	// 			} // otherwise, it is an information about loop scheduling
	// 			  // thus, it must not be treated as input data for data flow
	// 		} else if (auto st = dyn_cast<StoreInst>(&I)) {
	// 			mem_store.push_back(st);
	// 		}
	// 	}
	// }

	// load pattern
	check_all<0>(DA.get_loads(), AR.SE);
	// store pattern
	check_all<1>(DA.get_stores(), AR.SE);

	// //save the momory access insts
	// R.setMemLoad(std::move(mem_load));
	// R.setMemStore(std::move(mem_store));
}

template<int N, typename T>
bool VerifyAffineAGCompatiblePass::check_all(SmallVector<T*> &list,  ScalarEvolution &SE)
{
	// dump
	for (auto access : list) {
		access->dump();
	}

	// // verify each address pattern
	// for (auto access : list) {
	// 	auto addr = access->getOperand(N);
	// 	if (SE.isSCEVable(addr->getType())) {
	// 		auto *s = SE.getSCEV(addr);
	// 		s->dump();
	// 		parseSCEV(s, SE);
	// 	} else {
	// 		//not Scalar evolution
	// 	}
	// }
	return true;
}

void VerifyAffineAGCompatiblePass::parseSCEV(const SCEV *scev, ScalarEvolution &SE, int depth) {
	std::string indent =  std::string("\t", depth + 1);
	errs() << "type: " << scev->getSCEVType() << "\n";
	switch (scev->getSCEVType()) {
		case SCEVTypes::scAddRecExpr:
			{
				auto *SAR = dyn_cast<SCEVAddRecExpr>(scev);
				auto *start = SAR->getStart();
				auto *step = SAR->getStepRecurrence(SE);
				switch(step->getSCEVType()) {
					case SCEVTypes::scConstant:
						//OK
						errs() << "step: ";
						step->dump();
						break;
					default:
						//error
						errs() << "Step is not constant\n";
						break;
				}
			}
			break;
		case SCEVTypes::scAddExpr:
			{
				auto *SA = dyn_cast<SCEVAddExpr>(scev);
				SA->dump();
			}
			break;
		case SCEVTypes::scUnknown:
			{
				auto *V = dyn_cast<SCEVUnknown>(scev)->getValue();
				if (auto *arg = dyn_cast<Argument>(V)) {
					// it is data transfer between host and device
					// save symbol
					arg->print(errs() << indent);
					errs() << " is an arg\n";
				}
			}
		default:
			break;
	}
}

// void VerifyAffineAGCompatiblePass::parseSCEVAddRecExpr(const SCEVAddRecExpr *SAR,
// 													ScalarEvolution &SE)
// {
// 	auto *start = SAR->getStart();
// 	auto *step = SAR->getStepRecurrence(SE);
// 	errs() << "start: ";
// 	start->dump();
// 	errs() << "step: ";
// 	step->dump();

// 	if (auto childSARC = dyn_cast<SCEVCastExpr>(start)) {
// 		errs() << "start is constant\n";
// 	} else if (auto *childSAR = dyn_cast<SCEVAddRecExpr>(start)) {
// 		errs() << "start is rec\n";
// 	} else if (auto *childSA = dyn_cast<SCEVAddExpr>(start)) {
// 		errs() << "start is add expr\n";
// 		for (auto hoge : childSA->operands()) {
// 			errs() << "\t" << hoge->getSCEVType() << " ";
// 			if (auto *childchildSM = dyn_cast<SCEVMulExpr>(hoge)) {

// 			}
// 			hoge->dump();
// 			if (auto unknown = dyn_cast<SCEVUnknown>(hoge)) {
// 				errs() << "\t\tunknown ";
// 				parseSCUnknonw(unknown);
// 			}
// 		}
// 	}
// }

/* ================== Implementation of VerifyModulePass ================== */
PreservedAnalyses VerifyModulePass::run(Module &M, ModuleAnalysisManager &AM)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Start verification" << "\n");

	auto &MM = AM.getResult<ModelManagerPass>(M);
	auto model = MM.getModel();

	// obtain OpenMP kernels
	auto &kernels = AM.getResult<OmpKernelAnalysisPass>(M);

	// verify each OpenMP kernel
	for (auto &it : kernels) {
		//verify OpenMP target function
		auto *F = it.second;
		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		switch(model->getKind()) {
			case CGRAModel::CGRACategory::Decoupled:
				{auto verify_res = FAM.getResult<DecoupledVerifyPass>(*F);}
				break;
			case CGRAModel::CGRACategory::Generic:
				auto verify_res = FAM.getResult<GenericVerifyPass>(*F);
				break;
		}
	}

	// there is no modification so it must keep all analysis
	return PreservedAnalyses::all();
}

#undef DEBUG_TYPE

/* ================= Utility functions ================= */
LoopStandardAnalysisResults CGRAOmp::getLSAR(Function &F,
								FunctionAnalysisManager &AM)
{
	auto &AA = AM.getResult<AAManager>(F);
	auto &AC = AM.getResult<AssumptionAnalysis>(F);
	auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
	auto &LI = AM.getResult<LoopAnalysis>(F);
	auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
	auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
	auto &TTI = AM.getResult<TargetIRAnalysis>(F);
	BlockFrequencyInfo *BFI = &AM.getResult<BlockFrequencyAnalysis>(F);
	MemorySSA *MSSA = &AM.getResult<MemorySSAAnalysis>(F).getMSSA();

	return LoopStandardAnalysisResults(
		{AA, AC, DT, LI, SE, TLI, TTI, BFI, MSSA}
	);
}
