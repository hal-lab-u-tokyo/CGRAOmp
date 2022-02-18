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
*    Last Modified: 19-02-2022 08:40:13
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
#include "llvm/ADT/SetOperations.h"

#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"

#include "common.hpp"
#include "VerifyPass.hpp"
#include "CGRAOmpAnnotationPass.hpp"
#include "DecoupledAnalysis.hpp"
#include "AGVerifyPass.hpp"
#include "LoopDependencyAnalysis.hpp"

#include <system_error>
#include <functional>
#include <set>

using namespace llvm;
using namespace CGRAOmp;

#define DEBUG_TYPE "cgraomp"

static const char *VerboseDebug = DEBUG_TYPE "-verbose";


/* ================= Implementation of InstAvailability ================= */
void InstAvailability::print(raw_ostream &OS) const {
	std::set<StringRef> unsupported_opcode;

	for (auto inst : unsupported) {
		unsupported_opcode.insert(inst->getOpcodeName());
	}

	if (unsupported_opcode.size() > 0) {
		OS << formatv("Unsupported instructions are used: {0}",
			make_range(unsupported_opcode.begin(), unsupported_opcode.end()));
	} else {
		OS << "All instructions are supported\n";
	}
}

void InstAvailability::filter(SmallVector<Instruction*> *list)
{
	SmallPtrSet<Instruction*, 32> sub(list->begin(), list->end());
	//update (unsupported := unsupported - sub)
	set_subtract(unsupported, sub);
}

void InstAvailability::filter(SmallPtrSetImpl<Instruction*> *list)
{
	set_subtract(unsupported, *list);
}

/* ================= Implementation of VerifyInstAvailabilityPass ================= */

// Specilization of checkUnsupportedInst method for DecoupledVerifyPass
/**
 * @details For decoupled CGRAs, it verifies only computational part of instructions.
*/
template<>
Optional<SmallVector<Instruction*>> 
VerifyInstAvailabilityPass<DecoupledVerifyPass>::checkUnsupportedInst(Loop& L, 
								LoopAnalysisManager &AM,
								LoopStandardAnalysisResults &AR)
{

	auto LN = LoopNest::getLoopNest(L, AR.SE);
	auto innermost = LN->getInnermostLoop();

	auto &MM = AM.getResult<ModelManagerLoopProxy>(L, AR);
	auto *model = MM.getModel();

	InstList unsupported;

	auto DA = AM.getResult<DecoupledAnalysisPass>(L, AR);

	for (Value *v : DA.get_comps()) {
		if (auto *inst = dyn_cast<Instruction>(v)) {
			auto *imap = model->isSupported(inst);
			if (!imap) {
				unsupported.emplace_back(inst);
			}
		} else {
			LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX << "unexpected IR ";
						v->print(dbgs()));
		}
	}

	if (unsupported.size() > 0) {
		return Optional<InstList>(std::move(unsupported));
	} else {
		return None;
	}
}

template<>
AnalysisKey VerifyInstAvailabilityPass<DecoupledVerifyPass>::Key;

/* ===================== Implementation of VerifyResult ===================== */
void VerifyResult::print(raw_ostream &OS) const
{
	OS << "this is verifyResult\n";
}


/* ===================== Implementation of LoopVerifyResult ===================== */

void LoopVerifyResult::print(raw_ostream &OS) const
{
	OS << "this is LoopverifyResult\n";
}

bool LoopVerifyResult::bool_operator_impl()
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

/* ================== Implementation of TimeMultiplexedVerifyPass ================== */
AnalysisKey TimeMultiplexedVerifyPass::Key;

VerifyResult TimeMultiplexedVerifyPass::run(Function &F, FunctionAnalysisManager &AM)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Verifying "
				 << F.getName() << "for time-multiplexed CGRA\n");
	VerifyResult result;

	// get CGRA model
	auto MM = AM.getResult<ModelManagerFunctionProxy>(F);
	auto model = MM.getModel();
	auto gene_model = model->asDerived<TMCGRA>();

	assert(!"TimeMultiplexedVerifyPass is not implemented");

	return result;
}

/* ================= Implementation of DecoupledVerifyPass ================= */
AnalysisKey DecoupledVerifyPass::Key;

VerifyResult DecoupledVerifyPass::run(Function &F, FunctionAnalysisManager &AM)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Verifying a kernel for decoupled CGRA: "
				 << F.getName() << "\n");
	VerifyResult result;

	// get CGRA model
	auto MM = AM.getResult<ModelManagerFunctionProxy>(F);
	auto model = MM.getModel();
	auto dec_model = model->asDerived<DecoupledCGRA>();

	// ensure OmpStaticShecudleAnalysis result is cached for DecoupledAnalysis
	auto SI = AM.getResult<OmpStaticShecudleAnalysis>(F);
	if (!SI) {
		ExitOnError Exit(ERR_MSG_PREFIX);
		std::error_code EC;
		Exit(make_error<StringError>("Fail to find OpenMP scheduling info", EC));
	}

	// setup loop analysis manager
	auto AR = getLSAR(F, AM);
 	auto &LPM = AM.getResult<LoopAnalysisManagerFunctionProxy>(F).getManager();
	// get Kernel candidates
	auto loop_kernels = findPerfectlyNestedLoop(F, AR);

	if (loop_kernels.size() == 0) {
		LLVM_DEBUG(dbgs() << WARN_DEBUG_PREFIX << "Cannot find any valid loop kernels\n");
		return result;
	}
	DEBUG_WITH_TYPE(VerboseDebug,
		dbgs() << DBG_DEBUG_PREFIX << "The number of kernels " 
			   << loop_kernels.size() << "\n");


	for (auto L : loop_kernels) {
		LoopVerifyResult lvr;

		// verify decoupled result
		auto DA = LPM.getResult<DecoupledAnalysisPass>(*L, AR);
		std::string buf;
		raw_string_ostream OS(buf);
		DA.print(OS);
		auto DAR = DecoupleAnalysisResult(buf);
		lvr.setResult(&DAR);
		
		// excepted instructions for availability verification
		SmallPtrSet<Instruction*, 32> except_inst;

		// verify inter-loop depedency
		auto LD = LPM.getResult<LoopDependencyAnalysisPass>(*L, AR);
		switch (dec_model->getInterLoopDepType()) {
			case CGRAModel::InterLoopDep::No:
			{
				int dep_cout = LD.getNumDep() + LD.getNumMemDep();
				std::string msg;
				bool loop_dep_valid = true;
				if (dep_cout > 0)  {
					// invalid
					msg = formatv("including {0} inter loop dependencies", dep_cout);
					loop_dep_valid = false;
				} else {
					msg = "No dependency";
				}
				auto LDR = InterLoopDependencyAnalysisResult("No dependency");
				if (!loop_dep_valid) {
					LDR.setVio();
				}
				lvr.setResult(&LDR);
			}
			break;
			case CGRAModel::InterLoopDep::BackwardInst:
			{
				for (auto idv_dep : LD.idv_deps()) {
					except_inst.insert(idv_dep->getPhi());
				}
				for (auto dep : LD.lc_deps()) {
					except_inst.insert(dep->getPhi());
				}
			}
			break;
			default:
				llvm_unreachable("This type of capability for inter loop dependency is not implemented");
		}

		// verify instruction compatibility
		auto inst_avail = 
			LPM.getResult<VerifyInstAvailabilityPass<DecoupledVerifyPass>>(*L, AR);

		inst_avail.filter(&except_inst);
		if (!inst_avail) {
				LLVM_DEBUG(inst_avail.print(dbgs() << WARN_DEBUG_PREFIX);
				dbgs() << "\n";	);
		}
		lvr.setResult(&inst_avail);


		// verify conditional parts
		
		// verify each memory access
		VerifyResultBase *ag_compat;
		switch (dec_model->getAG()->getKind()) {
			case AddressGenerator::Kind::Affine:
				ag_compat = &(LPM.getResult<VerifyAGCompatiblePass<AddressGenerator::Kind::Affine>>(*L, AR));
				break;
			default:
				llvm_unreachable("This type of AG is not implemted\n");
		}
		lvr.setResult(ag_compat);

		// if the kernel passes all the verifications, it is registered
		if (lvr) {
			result.registerKernel(L, lvr);
		}

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
						"Detected perfectly nested loop in " << LN->getNestDepth()
						<< " nested loop kernel: " << inL->getName() <<
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


/* ================== Implementation of VerifyModulePass ================== */
PreservedAnalyses VerifyModulePass::run(Module &M, ModuleAnalysisManager &AM)
{
	LLVM_DEBUG(dbgs() << INFO_DEBUG_PREFIX << "Start verification" << "\n");

	auto &MM = AM.getResult<ModelManagerPass>(M);
	auto model = MM.getModel();

	// obtain OpenMP kernels
	auto &kernel_info = AM.getResult<OmpKernelAnalysisPass>(M);

	// verify each OpenMP kernel
	for (auto F : kernel_info.kernels()) {
		//verify OpenMP target function
		auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
		switch(model->getKind()) {
			case CGRAModel::CGRACategory::Decoupled:
				{auto verify_res = FAM.getResult<DecoupledVerifyPass>(*F);}
				break;
			case CGRAModel::CGRACategory::TimeMultiplexed:
				auto verify_res = FAM.getResult<TimeMultiplexedVerifyPass>(*F);
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

#undef DEBUG_TYPE